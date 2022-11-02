/*
 This source file is part of the Swift.org open source project

 Copyright (c) 2021-2022 Apple Inc. and the Swift project authors
 Licensed under Apache License v2.0 with Runtime Library Exception

 See https://swift.org/LICENSE.txt for license information
 See https://swift.org/CONTRIBUTORS.txt for Swift project authors
*/

import Foundation
import Markdown
import SymbolKit

/// A type that provides information about documentation bundles and their content.
public protocol DocumentationContextDataProvider {
    /// An object to notify when bundles are added or removed.
    var delegate: DocumentationContextDataProviderDelegate? { get set }
    
    /// The documentation bundles that this data provider provides.
    var bundles: [BundleIdentifier: DocumentationBundle] { get }
    
    /// Returns the data for the specified `url` in the provided `bundle`.
    ///
    /// - Parameters:
    ///   - url: The URL of the file to read.
    ///   - bundle: The bundle that the file is a part of.
    ///
    /// - Throws: When the file cannot be found in the workspace.
    func contentsOfURL(_ url: URL, in bundle: DocumentationBundle) throws -> Data
}

/// An object that responds to changes in available documentation bundles for a specific provider.
public protocol DocumentationContextDataProviderDelegate: AnyObject {
    
    /// Called when the `dataProvider` has added a new documentation bundle to its list of `bundles`.
    ///
    /// - Parameters:
    ///   - dataProvider: The provider that added this bundle.
    ///   - bundle: The bundle that was added.
    ///
    /// - Note: This method is called after the `dataProvider` has been added the bundle to its `bundles` property.
    func dataProvider(_ dataProvider: DocumentationContextDataProvider, didAddBundle bundle:  DocumentationBundle) throws
    
    /// Called when the `dataProvider` has removed a documentation bundle from its list of `bundles`.
    ///
    /// - Parameters:
    ///   - dataProvider: The provider that removed this bundle.
    ///   - bundle: The bundle that was removed.
    ///
    /// - Note: This method is called after the `dataProvider` has been removed the bundle from its `bundles` property.
    func dataProvider(_ dataProvider: DocumentationContextDataProvider, didRemoveBundle bundle:  DocumentationBundle) throws
}

/// Documentation bundles use a string value as a unique identifier.
///
/// This value is typically a reverse host name, for example: `com.<organization-name>.<product-name>`.
///
/// Documentation links may include the bundle identifier—as a host component of the URL—to reference content in a specific documentation bundle.
public typealias BundleIdentifier = String

/// The documentation context manages the in-memory model for the built documentation.
///
/// A ``DocumentationWorkspace`` discovers serialized documentation bundles from a variety of sources (files on disk, databases, or web services), provides them to the `DocumentationContext`,
/// and notifies the context when bundles are added or removed using the ``DocumentationContextDataProviderDelegate`` protocol.
///
/// When a documentation bundle is registered with the context, all of its content is loaded into memory and relationships between documentation entities are built. When this is done, the context can be queried
/// about documentation entities, resources, and relationships between entities.
///
/// ## Topics
///
/// ### Getting documentation resources
///
/// - ``entity(with:)``
/// - ``resource(with:trait:)``
///
/// ### Getting documentation relationships
///
/// - ``children(of:kind:)``
/// - ``parents(of:)``
///
public class DocumentationContext: DocumentationContextDataProviderDelegate {

    /// An error that's encountered while interacting with a ``SwiftDocC/DocumentationContext``.
    public enum ContextError: DescribedError {
        /// The node couldn't be found in the documentation context.
        case notFound(URL)
        
        /// The file wasn't UTF-8 encoded.
        case utf8StringDecodingFailed(url: URL)
        
        /// We allow a symbol declaration with no OS (for REST & Plist symbols)
        /// but if such a declaration is found the symbol can have only one declaration.
        case unexpectedEmptyPlatformName(String)
        
        /// The bundle registration operation is cancelled externally.
        case registrationDisabled
        
        public var errorDescription: String {
            switch self {
                case .notFound(let url):
                    return "Couldn't find the requested node '\(url)' in the documentation context."
                case .utf8StringDecodingFailed(let url):
                    return "The file at '\(url)' could not be read because it was not valid UTF-8."
                case .unexpectedEmptyPlatformName(let symbolIdentifier):
                    return "Declaration without operating system name for symbol \(symbolIdentifier) cannot be merged with more declarations with operating system name for the same symbol"
                case .registrationDisabled:
                    return "The bundle registration operation is cancelled externally."
            }
        }
    }
    
    /// A link resolver that resolves references by finding them in the documentation cache.
    let documentationCacheBasedLinkResolver = DocumentationCacheBasedLinkResolver()
    /// A link resolver that resolves references by finding them in path hierarchy.
    var hierarchyBasedLinkResolver: PathHierarchyBasedLinkResolver? = nil
    
    /// Gathered differences between the two link resolution implementations.
    ///
    /// This is only if ``LinkResolutionMigrationConfiguration/shouldReportLinkResolutionPathMismatches`` or  ``LinkResolutionMigrationConfiguration/shouldReportLinkResolutionResultMismatches`` is enabled.
    ///
    /// > Note: This is a temporary property that will go away along with the ``DocumentationCacheBasedLinkResolver`` at some point in the future.
    var linkResolutionMismatches = LinkResolutionMismatches()
    
    /// The provider of documentation bundles for this context.
    var dataProvider: DocumentationContextDataProvider
    
    /// The graph of all the documentation content and their relationships to each other.
    var topicGraph = TopicGraph()
    
    /// User-provided global options for this documentation conversion.
    var options: Options?

    /// A value to control whether the set of manually curated references found during bundle registration should be stored. Defaults to `false`. Setting this property to `false` clears any stored references from `manuallyCuratedReferences`.
    public var shouldStoreManuallyCuratedReferences: Bool = false {
        didSet {
            if shouldStoreManuallyCuratedReferences == false {
                manuallyCuratedReferences = nil
            }
        }
    }
    /// The set of all manually curated references if `shouldStoreManuallyCuratedReferences` was true at the time of processing and has remained `true` since.. Nil if curation has not been processed yet.
    public private(set) var manuallyCuratedReferences: Set<ResolvedTopicReference>?

    /// The root technology nodes of the Topic Graph.
    public var rootTechnologies: [ResolvedTopicReference] {
        return topicGraph.nodes.values.compactMap { node in
            guard node.kind == .technology && parents(of: node.reference).isEmpty else {
                return nil
            }
            return node.reference
        }
    }
    
    /// The root module nodes of the Topic Graph.
    ///
    /// This property is initialized during the registration of a documentation bundle.
    public private(set) var rootModules: [ResolvedTopicReference]!
        
    /// The topic reference of the root module, if it's the only registered module.
    var soleRootModuleReference: ResolvedTopicReference? {
        guard rootModules.count > 1 else {
            return rootModules.first
        }
        // There are multiple "root modules" but some may be "virtual".
        // Removing those may leave only one root module left.
        let nonVirtualModules = rootModules.filter {
            topicGraph.nodes[$0]?.isVirtual ?? false
        }
        return nonVirtualModules.count == 1 ? nonVirtualModules.first : nil
    }
        
    /// Map of document URLs to topic references.
    var documentLocationMap = BidirectionalMap<URL, ResolvedTopicReference>()
    /// A cache of already created documentation nodes for an already resolved reference.
    var documentationCache = [ResolvedTopicReference: DocumentationNode]()
    /// The asset managers for each documentation bundle, keyed by the bundle's identifier.
    var assetManagers = [BundleIdentifier: DataAssetManager]()
    /// A list of non-topic links that can be resolved.
    var nodeAnchorSections = [ResolvedTopicReference: AnchorSection]()
    
    /// A list of all the problems that was encountered while registering and processing the documentation bundles in this context.
    public var problems: [Problem] {
        return diagnosticEngine.problems
    }

    /// The engine that collects problems encountered while registering and processing the documentation bundles in this context.
    public var diagnosticEngine: DiagnosticEngine
    
    /// The dictionary mapping external documentation bundle identifiers to external reference resolvers.
    ///
    /// The context uses external reference resolvers to resolve links to documentation outside the context's registered bundles.
    ///
    /// - Note: Registering an external reference resolver for a bundle that is registered in the context is not allowed.
    /// Instead, see ``fallbackReferenceResolvers``.
    ///
    /// ## See Also
    /// - ``ExternalReferenceResolver``
    public var externalReferenceResolvers = [BundleIdentifier: ExternalReferenceResolver]()
    
    /// The dictionary mapping known documentation bundle identifiers to fallback reference resolvers.
    ///
    /// In situations where the local documentation context doesn't contain all the symbols or articles of a registered bundle—for example,
    /// when using a ``ConvertService`` that contains partial symbol graph information—the documentation context will look up locally
    /// unresolvable references using a fallback resolver (if one is set for the reference's bundle identifier).
    ///
    /// - Warning: Setting fallback reference resolver makes accesses to the context non-thread-safe. This is because performing
    /// external link resolution _after_ bundle registration mutates the context.
    ///
    /// ## See Also
    /// - ``ExternalReferenceResolver``
    public var fallbackReferenceResolvers = [BundleIdentifier: FallbackReferenceResolver]()
    
    /// A type that resolves symbols that are references from a symbol graph file but isn't included in any of the processed symbol graph files.
    ///
    /// - Note: Since it's not known what bundle these symbols belong to, there is only one resolver for all "external" symbols.
    public var externalSymbolResolver: ExternalSymbolResolver?
    
    /// The dictionary mapping known documentation bundle identifiers to fallback reference resolvers.
    ///
    /// In situations where the local documentation context doesn't contain all the assets of a registered bundle—for example, when
    /// using a ``ConvertService`` that contains partial contents of a bundle—the documentation context will look up locally
    /// unresolvable asset references using a fallback resolver (if one is set for the reference's bundle identifier.)
    public var fallbackAssetResolvers = [BundleIdentifier: FallbackAssetResolver]()
    
    /// All the symbol references that have been resolved from external sources.
    ///
    /// This is tracked to exclude external symbols from the build output. Information about external references is still included for the local pages that makes the external reference.
    var externallyResolvedSymbols = Set<ResolvedTopicReference>()
    
    /// All the link references that have been resolved from external sources, either successfully or not.
    var externallyResolvedLinks = [ValidatedURL: TopicReferenceResolutionResult]()
    
    /// The mapping of external symbol identifiers to known disambiguated symbol path components.
    ///
    /// In situations where the the local documentation context doesn't contain all of the current module's
    /// symbols, for example when using a ``ConvertService`` with a partial symbol graph,
    /// the documentation context is otherwise unable to accurately detect a collision for a given symbol and correctly
    /// disambiguate its path components. This value can be used to inject already disambiguated symbol
    /// path components into the documentation context.
    var knownDisambiguatedSymbolPathComponents: [String: [String]]?
    
    /// A temporary structure to hold a semantic value that hasn't yet had its links resolved.
    ///
    /// These temporary values are only expected to exist while the documentation is being built. Once the documentation bundles have been fully registered and the topic graph
    /// has been built, the documentation context shouldn't hold any semantic result values anymore.
    struct SemanticResult<S: Semantic> {
        /// The ``Semantic`` value with unresolved links.
        var value: S
        
        /// The source of the document that produces the ``value``.
        var source: URL
        
        /// The Topic Graph node for this value.
        var topicGraphNode: TopicGraph.Node
    }
    
    /// Temporary storage for articles before they are curated and moved to the documentation cache.
    ///
    /// This storage is only used while the documentation context is being built. Once the documentation bundles have been fully registered and the topic graph
    /// has been built, this list of uncurated articles will be empty.
    ///
    /// The key to lookup an article is the reference to the article itself.
    var uncuratedArticles = [ResolvedTopicReference: SemanticResult<Article>]()
    
    /// Temporary storage for documentation extension files before they are curated and moved to the documentation cache.
    ///
    /// This storage is only used while the documentation context is being built. Once the documentation bundles have been fully registered and the topic graph
    /// has been built, this list of uncurated documentation extensions will be empty.
    ///
    /// The key to lookup a documentation extension file is the symbol reference from its title (level 1 heading).
    ///
    /// - Warning: It's possible—but not supported—for multiple documentation extension files to specify the same symbol link.
    var uncuratedDocumentationExtensions = [ResolvedTopicReference: [SemanticResult<Article>]]()

    /// External metadata injected into the context, for example via command line arguments.
    public var externalMetadata = ExternalMetadata()
    
    
    /// The decoder used in the `SymbolGraphLoader`
    var decoder: JSONDecoder = JSONDecoder()
    
    /// Initializes a documentation context with a given `dataProvider` and registers all the documentation bundles that it provides.
    ///
    /// - Parameter dataProvider: The data provider to register bundles from.
    /// - Parameter diagnosticEngine: The pre-configured engine that will collect problems encountered during compilation.
    /// - Throws: If an error is encountered while registering a documentation bundle.
    public init(dataProvider: DocumentationContextDataProvider, diagnosticEngine: DiagnosticEngine = .init()) throws {
        self.dataProvider = dataProvider
        self.diagnosticEngine = diagnosticEngine
        self.dataProvider.delegate = self
        
        for bundle in dataProvider.bundles.values {
            try register(bundle)
        }
    }

    /// Initializes a documentation context with a given `dataProvider` and `diagnosticConsumers`
    /// and registers all the documentation bundles that `dataProvider` provides.
    ///
    /// - Parameters:
    ///     - dataProvider: The data provider to register bundles from.
    ///     - diagnosticEngine: The engine that will collect problems encountered during compilation.
    ///     - diagnosticConsumers: A collection of types that can consume diagnostics. These will be registered with `diagnosticEngine`.
    /// - Throws: If an error is encountered while registering a documentation bundle.
    @available(*, deprecated, message: "Use init(dataProvider:diagnosticEngine:) instead")
    public init(dataProvider: DocumentationContextDataProvider, diagnosticEngine: DiagnosticEngine = .init(), diagnosticConsumers: [DiagnosticConsumer]) throws {
        self.dataProvider = dataProvider
        self.diagnosticEngine = diagnosticEngine
        self.dataProvider.delegate = self

        for consumer in diagnosticConsumers {
            self.diagnosticEngine.add(consumer)
        }

        for bundle in dataProvider.bundles.values {
            try register(bundle)
        }
    }
    
    /// Respond to a new `bundle` being added to the `dataProvider` by registering it.
    ///
    /// - Parameters:
    ///   - dataProvider: The provider that added this bundle.
    ///   - bundle: The bundle that was added.
    public func dataProvider(_ dataProvider: DocumentationContextDataProvider, didAddBundle bundle: DocumentationBundle) throws {
        try benchmark(wrap: Benchmark.Duration(id: "bundle-registration")) {
            // Enable reference caching for this documentation bundle.
            ResolvedTopicReference.enableReferenceCaching(for: bundle.identifier)
            
            try self.register(bundle)
        }
    }
    
    /// Respond to a new `bundle` being removed from the `dataProvider` by unregistering it.
    ///
    /// - Parameters:
    ///   - dataProvider: The provider that removed this bundle.
    ///   - bundle: The bundle that was removed.
    public func dataProvider(_ dataProvider: DocumentationContextDataProvider, didRemoveBundle bundle: DocumentationBundle) throws {
        documentationCacheBasedLinkResolver.unregisterBundle(identifier: bundle.identifier)
        if let hierarchyBasedLinkResolver = hierarchyBasedLinkResolver {
            hierarchyBasedLinkResolver.unregisterBundle(identifier: bundle.identifier)
        }
        
        // Purge the reference cache for this bundle and disable reference caching for
        // this bundle moving forward.
        ResolvedTopicReference.purgePool(for: bundle.identifier)
        
        unregister(bundle)
    }
    
    /// The documentation bundles that are currently registered with the context.
    public var registeredBundles: Dictionary<String, DocumentationBundle>.Values {
        return dataProvider.bundles.values
    }
    
    /// Returns the `DocumentationBundle` with the given `identifier` if it's registered with the context, otherwise `nil`.
    public func bundle(identifier: String) -> DocumentationBundle? {
        return dataProvider.bundles[identifier]
    }
        
    /// Perform semantic analysis on a given `document` at a given `source` location and append any problems found to `problems`.
    ///
    /// - Parameters:
    ///   - document: The document to analyze.
    ///   - source: The location of the document.
    ///   - bundle: The bundle that the document belongs to.
    ///   - problems: A mutable collection of problems to update with any problem encountered during the semantic analysis.
    /// - Returns: The result of the semantic analysis.
    private func analyze(_ document: Document, at source: URL, in bundle: DocumentationBundle, engine: DiagnosticEngine) -> Semantic? {
        var analyzer = SemanticAnalyzer(source: source, context: self, bundle: bundle)
        let result = analyzer.visit(document)
        engine.emit(analyzer.problems)
        return result
    }
    
    /// Perform global analysis of compiled Markup
    ///
    /// Global analysis differs from semantic analysis in that no transformation is expected to occur. The
    /// analyses performed in this method don't transform documents, they only inspect them.
    ///
    /// Global checks are generally not expected to be run on tutorials or tutorial articles. The structure of
    /// tutorial content is very different from the expected structure of most documentation. If a checker is
    /// only checking content, it can probably be run on all types of documentation without issue. If the
    /// checker needs to check (or makes assumptions about) structure, it should probably be run only on
    /// non-tutorial content. If tutorial-related docs need to be checked or analyzed in some way (such as
    /// checking for the existence of a child directive), a semantic analyzer is probably the better solution.
    /// Tutorial content is highly structured and will be parsed into models that can be analyzed in a
    /// type-safe manner.
    ///
    /// - Parameters:
    ///   - document: The document to analyze.
    ///   - source: The location of the document.
    private func check(_ document: Document, at source: URL) {
        var checker = CompositeChecker([
            AbstractContainsFormattedTextOnly(sourceFile: source).any(),
            DuplicateTopicsSections(sourceFile: source).any(),
            InvalidAdditionalTitle(sourceFile: source).any(),
            MissingAbstract(sourceFile: source).any(),
            NonOverviewHeadingChecker(sourceFile: source).any(),
            SeeAlsoInTopicsHeadingChecker(sourceFile: source).any(),
            TopicsSectionWithoutSubheading(sourceFile: source).any(),
        ])
        checker.visit(document)
        diagnosticEngine.emit(checker.problems)
    }
    
    /// A cache of plain string module names, keyed by the module node reference.
    private var moduleNameCache: [ResolvedTopicReference: (displayName: String, symbolName: String)] = [:]
    
    /// Find the known plain string module name for a given module reference.
    ///
    /// - Note: Looking up module names requires that the module names have been pre-resolved. This happens automatically at the end of bundle registration.
    ///
    /// - Parameter moduleReference: The module reference to find the module name for.
    /// - Returns: The plain string name for the referenced module.
    func moduleName(forModuleReference moduleReference: ResolvedTopicReference) -> (displayName: String, symbolName: String) {
        if let name = moduleNameCache[moduleReference]  {
            return name
        }
        // If no name is found it's considered a programmer error; either that the names haven't been resolved yet
        // or that the passed argument isn't a reference to a known module.
        if moduleNameCache.isEmpty {
            fatalError("Incorrect use of API: '\(#function)' requires that bundles have finished registering.")
        }
        fatalError("Incorrect use of API: '\(#function)' can only be used with known module references")
    }
    
    /// Attempts to resolve the module names of all root modules.
    ///
    /// This allows the module names to quickly be looked up using ``moduleName(forModuleReference:)``
    func preResolveModuleNames() {
        for reference in rootModules {
            if let node = try? entity(with: reference) {
                let displayName: String
                switch node.name {
                case .conceptual(let title):
                    displayName = title
                case .symbol(let declaration):
                    displayName = declaration.tokens.map { $0.description }.joined()
                }
                // A module node should always have a symbol.
                // Remove the fallback value and force unwrap `node.symbol` on the main branch: https://github.com/apple/swift-docc/issues/249
                moduleNameCache[reference] = (displayName, node.symbol?.names.title ?? reference.lastPathComponent)
            }
        }
    }
    
    /// A documentation node with resolved content and any relevant problems.
    private typealias ResolvedSymbolResultWithProblems = (node: DocumentationNode, preciseIdentifier: String, problems: [Problem])

    /// A temporary storage type for an external link.
    private struct ExternalLinkResult: Hashable {
        let unresolved: UnresolvedTopicReference
        let targetLanguage: SourceLanguage
    }
    
    /// Attempts to resolve links external to the given bundle.
    ///
    /// The resolved references are collected in `externallyResolvedLinks`.
    ///
    /// - Parameters:
    ///   - references: The references of the nodes to walk.
    ///   - bundle: The documentation bundle to resolve links against.
    /// - Throws: Rethrows any errors that a registered external resolver might throw.
    /// > Note: References that don't have matching node in `documentationCache` are ignored.
    private func preResolveExternalLinks(references: [ResolvedTopicReference], bundle: DocumentationBundle) throws {
        try preResolveExternalLinks(semanticObjects: references.compactMap({ reference -> ReferencedSemanticObject? in
            guard let node = try? entity(with: reference), let semantic = node.semantic else { return nil }
            return (reference: reference, semantic: semantic)
        }), bundle: bundle)
    }
    
    /// A tuple of a semantic object and its reference in the topic graph.
    typealias ReferencedSemanticObject = (reference: ResolvedTopicReference, semantic: Semantic)
    
    /// Converts a semantic result to a referenced semantic object by removing the generic constraint.
    private func referencedSemanticObject<S: Semantic>(from: SemanticResult<S>) -> ReferencedSemanticObject {
        return (reference: from.topicGraphNode.reference, semantic: from.value)
    }
    
    /// Attempts to resolve links external to the given bundle by visiting the given list of semantic objects.
    ///
    /// The resolved references are collected in `externallyResolvedLinks`.
    ///
    /// - Parameters:
    ///   - semanticObjects: A list of semantic objects to visit to collect links.
    ///   - bundle: The documentation bundle to resolve links against.
    private func preResolveExternalLinks(semanticObjects: [ReferencedSemanticObject], bundle: DocumentationBundle) throws {
        // If there are no external resolvers added we will not resolve any links.
        guard !externalReferenceResolvers.isEmpty else { return }
        
        let collectedExternalLinks: [ExternalLinkResult] = semanticObjects.concurrentPerform { object, results in
            autoreleasepool {
                // Walk the node and extract external link references.
                var externalLinksCollector = ExternalReferenceWalker(bundle: bundle)
                externalLinksCollector.visit(object.semantic)
                
                // Add the link pairs to `collectedExternalLinks`.
                results.append(contentsOf:
                    externalLinksCollector.collectedExternalReferences.map { unresolved -> ExternalLinkResult in
                        return ExternalLinkResult(unresolved: unresolved, targetLanguage: object.reference.sourceLanguage)
                    }
                )
            }
        }
        
        try Set(collectedExternalLinks).compactMap { externalLink -> (url: ValidatedURL, resolved: TopicReferenceResolutionResult)? in
            guard let referenceBundleIdentifier = externalLink.unresolved.topicURL.components.host else {
                assertionFailure("Should not hit this code path, url is verified to have an external bundle id.")
                return nil
            }
            
            if let externalResolver = externalReferenceResolvers[referenceBundleIdentifier] {
                let reference = externalResolver.resolve(.unresolved(externalLink.unresolved), sourceLanguage: externalLink.targetLanguage)
                if case .success(let resolvedReference) = reference {
                    // Add the resolved entity to the documentation cache.
                    if let externallyResolvedNode = try externalEntity(with: resolvedReference) {
                        documentationCache[resolvedReference] = externallyResolvedNode
                    }
                }
                return (externalLink.unresolved.topicURL, reference)
            }
            
            return nil
        }
        .forEach { pair in
            externallyResolvedLinks[pair.url] = pair.resolved
            if case .success(let resolvedReference) = pair.resolved,
                pair.url.absoluteString != resolvedReference.absoluteString,
                let url = ValidatedURL(resolvedReference.url) {
                // If the resolved reference has a different URL than the link cache both URLs
                // so we can resolve both unresolved and resolved references.
                externallyResolvedLinks[url] = pair.resolved
            }
        }
    }
    
    /// A resolved documentation node along with any relevant problems.
    private typealias LinkResolveResult = (reference: ResolvedTopicReference, node: DocumentationNode, problems: [Problem])

    /**
     Attempt to resolve links in curation-only documentation, converting any ``TopicReferences`` from `.unresolved` to `.resolved` where possible.
     */
    private func resolveLinks(curatedReferences: Set<ResolvedTopicReference>, bundle: DocumentationBundle) {
        let references = Array(curatedReferences)
        let results = Synchronized<[LinkResolveResult]>([])
        results.sync({ $0.reserveCapacity(references.count) })

        let resolveNodeWithReference: (ResolvedTopicReference) -> Void = { [unowned self] reference in
            if var documentationNode = try? entity(with: reference), documentationNode.semantic is Article || documentationNode.semantic is Symbol {
                for doc in documentationNode.docChunks {
                    let source: URL?
                    switch doc.source {
                    case _ where documentationNode.semantic is Article,
                            .documentationExtension:
                        source = documentLocationMap[reference]
                    case .sourceCode(location: let location):
                        // For symbols, first check if we should reference resolve
                        // inherited docs or not. If we don't inherit the docs
                        // we should also skip reference resolving the chunk.
                        if let semantic = documentationNode.semantic as? Symbol,
                           semantic.origin != nil, !externalMetadata.inheritDocs {
                            
                            // If the two symbols are coming from different modules,
                            // regardless if they are in the same bundle
                            // (for example Foundation and SwiftUI), skip link resolving.
                            if let originSymbol = symbolIndex[semantic.origin!.identifier]?.semantic as? Symbol,
                               originSymbol.moduleReference != semantic.moduleReference {
                                continue
                            }
                        }

                        source = location?.url()
                    }

                    // Find the inheritance parent, if the docs are inherited.
                    let inheritanceParentReference: ResolvedTopicReference?
                    if let semantic = documentationNode.semantic as? Symbol,
                        semantic.origin != nil,
                        let originNode = symbolIndex[semantic.origin!.identifier] {
                        inheritanceParentReference = originNode.reference
                    } else {
                        inheritanceParentReference = nil
                    }

                    var resolver = ReferenceResolver(context: self, bundle: bundle, source: source, rootReference: reference, inheritanceParentReference: inheritanceParentReference)
                    
                    // Update the cache with the resolved node.
                    // We aggressively release used memory, since we're copying all semantic objects
                    // on the line below while rewriting nodes with the resolved content.
                    documentationNode.semantic = autoreleasepool { resolver.visit(documentationNode.semantic) }
                    
                    let pageImageProblems = documentationNode.metadata?.pageImages.compactMap { pageImage in
                        return resolver.resolve(
                            resource: pageImage.source,
                            range: pageImage.originalMarkup.range,
                            severity: .warning
                        )
                    } ?? []
                    
                    resolver.problems.append(contentsOf: pageImageProblems)

                    var problems = resolver.problems

                    if case DocumentationNode.DocumentationChunk.Source.sourceCode = doc.source,
                       let docs = documentationNode.symbol?.docComment {
                        // Offset all problem ranges by the start location of the
                        // source comment in the context of the complete file.
                        problems = problems.compactMap { problem -> Problem? in
                            guard let docRange = docs.lines.first?.range else { return nil }
                            return Problem(diagnostic: problem.diagnostic.offsetedWithRange(docRange),
                                           possibleSolutions: problem.possibleSolutions)
                        }
                    }

                    let result: LinkResolveResult = (reference: reference, node: documentationNode, problems: problems)
                    results.sync({ $0.append(result) })
                }
            }
        }

        // Resolve links concurrently if there are no external resolvers.
        references.concurrentPerform { reference -> Void in
            resolveNodeWithReference(reference)
        }

        for result in results.sync({ $0 }) {
            documentationCache[result.reference] = result.node
            if let preciseIdentifier = result.node.symbol?.identifier.precise {
                symbolIndex[preciseIdentifier] = result.node
            }
            diagnosticEngine.emit(result.problems)
        }
    }
    
    /// Attempt to resolve links in imported documentation, converting any ``TopicReferences`` from `.unresolved` to `.resolved` where possible.
    ///
    /// This function is passed pages that haven't been added to the topic graph yet. Calling this function will load the documentation entity for each of these pages
    /// and add nodes and relationships for some in-page semantics the `topicGraph`. After calling this function, these pages should be accessed by looking them
    /// up in the context, not from the arrays that was passed as arguments.
    ///
    /// - Parameters:
    ///   - technologies: The list of temporary 'technology' pages.
    ///   - tutorials: The list of temporary 'tutorial' pages.
    ///   - tutorialArticles: The list of temporary 'tutorialArticle' pages.
    ///   - bundle: The bundle to resolve links against.
    private func resolveLinks(technologies: [SemanticResult<Technology>],
                              tutorials: [SemanticResult<Tutorial>],
                              tutorialArticles: [SemanticResult<TutorialArticle>],
                              bundle: DocumentationBundle) {
        
        let sourceLanguages = soleRootModuleReference.map { self.sourceLanguages(for: $0) } ?? [.swift]

        // Technologies
        
        for technologyResult in technologies {
            autoreleasepool {
                let url = technologyResult.source
                let unresolvedTechnology = technologyResult.value
                var resolver = ReferenceResolver(context: self, bundle: bundle, source: url)
                let technology = resolver.visit(unresolvedTechnology) as! Technology
                diagnosticEngine.emit(resolver.problems)
                
                // Add to document map
                documentLocationMap[url] = technologyResult.topicGraphNode.reference
                
                let technologyReference = technologyResult.topicGraphNode.reference.withSourceLanguages(sourceLanguages)
                
                let technologyNode = DocumentationNode(
                    reference: technologyReference,
                    kind: .technology,
                    sourceLanguage: Self.defaultLanguage(in: sourceLanguages),
                    availableSourceLanguages: sourceLanguages,
                    name: .conceptual(title: technology.intro.title),
                    markup: technology.originalMarkup,
                    semantic: technology
                )
                documentationCache[technologyReference] = technologyNode
                
                // Update the reference in the topic graph with the technology's available languages.
                topicGraph.updateReference(
                    technologyResult.topicGraphNode.reference,
                    newReference: technologyReference
                )

                let anonymousVolumeName = "$volume"
                
                for volume in technology.volumes {
                    // Graph node: Volume
                    let volumeReference = technologyNode.reference.appendingPath(volume.name ?? anonymousVolumeName)
                    let volumeNode = TopicGraph.Node(reference: volumeReference, kind: .volume, source: .file(url: url), title: volume.name ?? anonymousVolumeName)
                    topicGraph.addNode(volumeNode)
                    
                    // Graph edge: Technology -> Volume
                    topicGraph.addEdge(from: technologyResult.topicGraphNode, to: volumeNode)
                    
                    for chapter in volume.chapters {
                        // Graph node: Module
                        let baseNodeReference: ResolvedTopicReference
                        if volume.name == nil {
                            baseNodeReference = technologyNode.reference
                        } else {
                            baseNodeReference = volumeNode.reference
                        }

                        let chapterReference = baseNodeReference.appendingPath(chapter.name)
                        let chapterNode = TopicGraph.Node(reference: chapterReference, kind: .chapter, source: .file(url: url), title: chapter.name)
                        topicGraph.addNode(chapterNode)
                        
                        // Graph edge: Volume -> Chapter
                        topicGraph.addEdge(from: volumeNode, to: chapterNode)
                        
                        for tutorialReference in chapter.topicReferences {
                            guard case let .resolved(.success(tutorialReference)) = tutorialReference.topic,
                                let tutorialNode = topicGraph.nodeWithReference(tutorialReference) else {
                                    continue
                            }
                            // Graph edge: Chapter -> Tutorial | TutorialArticle
                            topicGraph.addEdge(from: chapterNode, to: tutorialNode)
                        }
                    }
                }
            }
        }
        
        // Tutorials
        
        for tutorialResult in tutorials {
            autoreleasepool {
                let url = tutorialResult.source
                let unresolvedTutorial = tutorialResult.value
                var resolver = ReferenceResolver(context: self, bundle: bundle, source: url)
                let tutorial = resolver.visit(unresolvedTutorial) as! Tutorial
                diagnosticEngine.emit(resolver.problems)
                
                // Add to document map
                documentLocationMap[url] = tutorialResult.topicGraphNode.reference
                
                let tutorialReference = tutorialResult.topicGraphNode.reference.withSourceLanguages(sourceLanguages)
                
                let tutorialNode = DocumentationNode(
                    reference: tutorialReference,
                    kind: .tutorial,
                    sourceLanguage: Self.defaultLanguage(in: sourceLanguages),
                    availableSourceLanguages: sourceLanguages,
                    name: .conceptual(title: tutorial.intro.title),
                    markup: tutorial.originalMarkup,
                    semantic: tutorial
                )
                documentationCache[tutorialReference] = tutorialNode
                
                // Update the reference in the topic graph with the tutorial's available languages.
                topicGraph.updateReference(
                    tutorialResult.topicGraphNode.reference,
                    newReference: tutorialReference
                )
            }
        }
        
        // Tutorial Articles
        
        for articleResult in tutorialArticles {
            autoreleasepool {
                let url = articleResult.source
                let unresolvedTutorialArticle = articleResult.value
                var resolver = ReferenceResolver(context: self, bundle: bundle, source: url)
                let article = resolver.visit(unresolvedTutorialArticle) as! TutorialArticle
                diagnosticEngine.emit(resolver.problems)
                            
                // Add to document map
                documentLocationMap[url] = articleResult.topicGraphNode.reference

                let articleReference = articleResult.topicGraphNode.reference.withSourceLanguages(sourceLanguages)
                
                let articleNode = DocumentationNode(
                    reference: articleReference,
                    kind: .tutorialArticle,
                    sourceLanguage: Self.defaultLanguage(in: sourceLanguages),
                    availableSourceLanguages: sourceLanguages,
                    name: .conceptual(title: article.title ?? ""),
                    markup: article.originalMarkup,
                    semantic: article
                )
                documentationCache[articleReference] = articleNode
                
                // Update the reference in the topic graph with the article's available languages.
                topicGraph.updateReference(
                    articleResult.topicGraphNode.reference,
                    newReference: articleReference
                )
            }
        }
        
        // Articles are resolved in a separate pass
    }
    
    private func registerDocuments(from bundle: DocumentationBundle) throws -> (
        technologies: [SemanticResult<Technology>],
        tutorials: [SemanticResult<Tutorial>],
        tutorialArticles: [SemanticResult<TutorialArticle>],
        articles: [SemanticResult<Article>]
    ) {
        // First, try to understand the basic structure of the document by
        // analyzing it and putting references in as "unresolved".
        var technologies = [SemanticResult<Technology>]()
        var tutorials = [SemanticResult<Tutorial>]()
        var tutorialArticles = [SemanticResult<TutorialArticle>]()
        var articles = [SemanticResult<Article>]()
        
        var references: [ResolvedTopicReference: URL] = [:]

        let decodeError = Synchronized<Error?>(nil)
        
        // Load and analyze documents concurrently
        let analyzedDocuments: [(URL, Semantic)] = bundle.markupURLs.concurrentPerform { url, results in
            guard decodeError.sync({ $0 == nil }) else { return }
            
            do {
                let data = try dataProvider.contentsOfURL(url, in: bundle)
                let source = String(decoding: data, as: UTF8.self)
                let document = Document(parsing: source, source: url, options: [.parseBlockDirectives, .parseSymbolLinks])
                
                // Check for non-inclusive language in all types of docs if that diagnostic severity is required.
                if externalMetadata.diagnosticLevel >= NonInclusiveLanguageChecker.severity {
                    var langChecker = NonInclusiveLanguageChecker(sourceFile: url)
                    langChecker.visit(document)
                    diagnosticEngine.emit(langChecker.problems)
                }

                guard let analyzed = analyze(document, at: url, in: bundle, engine: diagnosticEngine) else {
                    return
                }
                
                // Only check non-tutorial documents from markup.
                if analyzed is Article {
                    check(document, at: url)
                }
                
                results.append((url, analyzed))
            } catch {
                decodeError.sync({ $0 = error })
            }
        }
        
        // Rethrow the decoding error if decoding failed.
        if let error = decodeError.sync({ $0 }) {
            throw error
        }
        
        // to preserve the order of documents by url
        let analyzedDocumentsSorted = analyzedDocuments.sorted(by: \.0.absoluteString)

        for analyzedDocument in analyzedDocumentsSorted {
            // Store the references we encounter to ensure they're unique. The file name is currently the only part of the URL considered for the topic reference, so collisions may occur.
            let (url, analyzed) = analyzedDocument

            let path = NodeURLGenerator.pathForSemantic(analyzed, source: url, bundle: bundle)
            let reference = ResolvedTopicReference(bundleIdentifier: bundle.identifier, path: path, sourceLanguage: .swift)
            
            if let firstFoundAtURL = references[reference] {
                let problem = Problem(
                    diagnostic: Diagnostic(
                        source: url,
                        severity: .warning,
                        range: nil,
                        identifier: "org.swift.docc.DuplicateReference",
                        summary: """
                        Redeclaration of '\(firstFoundAtURL.lastPathComponent)'; this file will be skipped
                        """,
                        explanation: """
                        This content was already declared at '\(firstFoundAtURL)'
                        """
                    ),
                    possibleSolutions: []
                )
                diagnosticEngine.emit(problem)
                continue
            }
            
            references[reference] = url
            
            /*
             Add all topic graph nodes up front before resolution starts, because
             there may be circular linking.
             */
            if let technology = analyzed as? Technology {
                let topicGraphNode = TopicGraph.Node(reference: reference, kind: .technology, source: .file(url: url), title: technology.intro.title)
                topicGraph.addNode(topicGraphNode)
                let result = SemanticResult(value: technology, source: url, topicGraphNode: topicGraphNode)
                technologies.append(result)
            } else if let tutorial = analyzed as? Tutorial {
                let topicGraphNode = TopicGraph.Node(reference: reference, kind: .tutorial, source: .file(url: url), title: tutorial.title ?? "")
                topicGraph.addNode(topicGraphNode)
                let result = SemanticResult(value: tutorial, source: url, topicGraphNode: topicGraphNode)
                tutorials.append(result)
                
                insertLandmarks(tutorial.landmarks, from: topicGraphNode, source: url)
            } else if let tutorialArticle = analyzed as? TutorialArticle {
                let topicGraphNode = TopicGraph.Node(reference: reference, kind: .tutorialArticle, source: .file(url: url), title: tutorialArticle.title ?? "")
                topicGraph.addNode(topicGraphNode)
                let result = SemanticResult(value: tutorialArticle, source: url, topicGraphNode: topicGraphNode)
                tutorialArticles.append(result)
                
                insertLandmarks(tutorialArticle.landmarks, from: topicGraphNode, source: url)
            } else if let article = analyzed as? Article {
                                
                // Here we create a topic graph node with the prepared data but we don't add it to the topic graph just yet
                // because we don't know where in the hierarchy the article belongs, we will add it later when crawling the manual curation via Topics task groups.
                let topicGraphNode = TopicGraph.Node(reference: reference, kind: .article, source: .file(url: url), title: article.title!.plainText)
                let result = SemanticResult(value: article, source: url, topicGraphNode: topicGraphNode)
                
                // Separate articles that look like documentation extension files from other articles, so that the documentation extension files can be matched up with a symbol.
                // At this point we consider all articles with an H1 containing link "documentation extension" - some links might not resolve in the final documentation hierarchy
                // and we will emit warnings for those later on when we finalize the bundle discovery phase.
                if let link = result.value.title?.child(at: 0) as? AnyLink,
                   let url = link.destination.flatMap(ValidatedURL.init(parsingExact:)) {
                    let reference = result.topicGraphNode.reference
                    
                    let symbolPath = NodeURLGenerator.Path.documentation(path: url.components.path).stringValue
                    let symbolReference = ResolvedTopicReference(
                        bundleIdentifier: reference.bundleIdentifier,
                        path: symbolPath,
                        fragment: nil,
                        sourceLanguages: reference.sourceLanguages
                    )
                    
                    uncuratedDocumentationExtensions[symbolReference, default: []].append(result)
                    
                    // Warn for an incorrect root page metadata directive.
                    if result.value.metadata?.technologyRoot != nil {
                        let diagnostic = Diagnostic(source: url.url, severity: .warning, range: article.metadata?.technologyRoot?.originalMarkup.range, identifier: "org.swift.docc.UnexpectedTechnologyRoot", summary: "Don't use TechnologyRoot in documentation extension files because it's only valid as a directive in articles")
                        let problem = Problem(diagnostic: diagnostic, possibleSolutions: [])
                        diagnosticEngine.emit(problem)
                    }
                } else {
                    precondition(uncuratedArticles[result.topicGraphNode.reference] == nil, "Article references are unique.")
                    uncuratedArticles[result.topicGraphNode.reference] = result
                    articles.append(result)
                }
            } else {
                let topLevelDirectives = BlockDirective.topLevelDirectiveNames
                    .map { $0.singleQuoted }
                    .list(finalConjunction: .or)
                let explanation = """
                    File contains unexpected markup at top level. Expected only one of \(topLevelDirectives) directives at the top level
                    """
                let zeroLocation = SourceLocation(line: 1, column: 1, source: nil)
                let diagnostic = Diagnostic(source: url, severity: .warning, range: zeroLocation..<zeroLocation, identifier: "org.swift.docc.UnexpectedTopLevelMarkup", summary: explanation)
                let problem = Problem(diagnostic: diagnostic, possibleSolutions: [])
                diagnosticEngine.emit(problem)
            }
        }
        
        return (technologies, tutorials, tutorialArticles, articles)
    }
    
    private func insertLandmarks<Landmarks: Sequence>(_ landmarks: Landmarks, from topicGraphNode: TopicGraph.Node, source url: URL) where Landmarks.Element == Landmark {
        for landmark in landmarks {
            guard let range = landmark.range else {
                continue
            }
            
            let landmarkReference = topicGraphNode.reference.withFragment(landmark.title)
            
            // Graph node: Landmark
            let landmarkTopicGraphNode = TopicGraph.Node(reference: landmarkReference, kind: .onPageLandmark, source: .range(range, url: url), title: landmark.title)
            topicGraph.addNode(landmarkTopicGraphNode)
            
            // Graph edge: Topic -> Landmark
            topicGraph.addEdge(from: topicGraphNode, to: landmarkTopicGraphNode)
            
            documentationCache[landmarkReference] = DocumentationNode(reference: landmarkReference, kind: .onPageLandmark, sourceLanguage: .swift, name: .conceptual(title: landmark.title), markup: landmark.markup, semantic: nil)
        }
    }
    
    /// A lookup of symbol documentation nodes based on the symbol's precise identifier.
    ///
    /// This allows convenient access to other symbol's documentation nodes while building relationships between symbols.
    private(set) var symbolIndex = [String: DocumentationNode]()
    
    private func nodeWithInitializedContent(reference: ResolvedTopicReference, matches: [DocumentationContext.SemanticResult<Article>]?) -> DocumentationNode {
        precondition(documentationCache.keys.contains(reference))
        
        // A symbol can have only one documentation extension markdown file, so emit warnings if there are more.
        if let matches = matches, matches.count > 1 {
            let zeroRange = SourceLocation(line: 1, column: 1, source: nil)..<SourceLocation(line: 1, column: 1, source: nil)
            for match in matches {
                let range = match.value.title?.range ?? zeroRange
                let problem = Problem(diagnostic: Diagnostic(source: match.source, severity: .warning, range: range, identifier: "org.swift.docc.DuplicateMarkdownTitleSymbolReferences", summary: "Multiple occurrences of \(reference.path.singleQuoted) found", explanation: "Only one documentation extension file should reference the symbol \(reference.path.singleQuoted) in its title."), possibleSolutions: [])
                diagnosticEngine.emit(problem)
            }
        }

        var updatedNode = documentationCache[reference]!
        
        // Pull a matched article out of the cache and attach content to the symbol
        let symbol = updatedNode.unifiedSymbol?.documentedSymbol
        let foundDocumentationExtension = matches?.first
        
        updatedNode.initializeSymbolContent(
            documentationExtension: foundDocumentationExtension?.value,
            engine: diagnosticEngine
        )

        // After merging the documentation extension into the symbol, warn about deprecation summary for non-deprecated symbols.
        if let foundDocumentationExtension = foundDocumentationExtension,
            foundDocumentationExtension.value.deprecationSummary != nil,
            (updatedNode.semantic as? Symbol)?.isDeprecated == false,
            let articleMarkup = foundDocumentationExtension.value.markup,
            let symbol = symbol
        {
            let directive = articleMarkup.children.mapFirst { child -> BlockDirective? in
                guard let directive = child as? BlockDirective, directive.name == DeprecationSummary.directiveName else { return nil }
                return directive
            }
            diagnosticEngine.emit(Problem(diagnostic: Diagnostic(source: foundDocumentationExtension.source, severity: .warning, range: directive?.range, identifier: "org.swift.docc.DeprecationSummaryForAvailableSymbol", summary: "\(symbol.absolutePath.singleQuoted) isn't unconditionally deprecated"), possibleSolutions: []))
        }

        return updatedNode
    }
    
    /// Creates a topic graph node and a documentation node for the given symbol.
    private func preparedSymbolData(_ symbol: UnifiedSymbolGraph.Symbol, reference: ResolvedTopicReference, module: SymbolGraph.Module, moduleReference: ResolvedTopicReference, bundle: DocumentationBundle, fileURL symbolGraphURL: URL?) -> AddSymbolResultWithProblems {
        let documentation = DocumentationNode(reference: reference, unifiedSymbol: symbol, moduleData: module, moduleReference: moduleReference)
        let source: TopicGraph.Node.ContentLocation // TODO: use a list of URLs for the files in a unified graph
        if let symbolGraphURL = symbolGraphURL {
            source = .file(url: symbolGraphURL)
        } else {
            source = .external
        }
        let graphNode = TopicGraph.Node(reference: reference, kind: documentation.kind, source: source, title: symbol.defaultSymbol!.names.title, isVirtual: module.isVirtual)

        return ((reference, symbol.uniqueIdentifier, graphNode, documentation), [])
    }

    private func parentChildRelationship(from edge: SymbolGraph.Relationship) -> (ResolvedTopicReference, ResolvedTopicReference)? {
        // Filter only parent <-> child edges
        switch edge.kind {
        case .memberOf, .requirementOf, .declaredIn, .inContextOf:
            guard let parentRef = symbolIndex[edge.target]?.reference, let childRef = symbolIndex[edge.source]?.reference else {
            return nil
            }
            return (parentRef, childRef)
        default: return nil
        }
    }

    static private func sortRelationshipsPreOrder(lhs: (ResolvedTopicReference, ResolvedTopicReference), rhs: (ResolvedTopicReference, ResolvedTopicReference)) -> Bool {
        // To walk the relationships deterministically for nodes at the same level in the hierarchy sort alphabetically.
        if lhs.0.pathComponents.count == rhs.0.pathComponents.count {
            return lhs.0.path < rhs.0.path
        }
        return lhs.0.pathComponents.count < rhs.0.pathComponents.count
    }

    /// The result of converting a symbol into a documentation node.
    private typealias AddSymbolResult = (reference: ResolvedTopicReference, preciseIdentifier: String, topicGraphNode: TopicGraph.Node, node: DocumentationNode)
    /// An optional result of converting a symbol into a documentation along with any related problems.
    private typealias AddSymbolResultWithProblems = (AddSymbolResult, problems: [Problem])
    
    /// Concurrently adds a symbol to the graph, index, and cache, or replaces an existing symbol with the same precise identifier
    /// (for light updates to symbols already in the graph).
    ///
    /// In case an article with a title that matches the symbol link is found in the article cache, it is removed from the cache
    /// and its content is merged with the symbol's metadata and/or documentation before the symbol is finally added to the topic graph.
    ///
    /// ```
    ///            ┌──────────────────┐
    ///            │   Topic Graph    │◀──────────────────────┐
    ///            └──────────────────┘                       │
    ///  ┌──────────────────┐             .───────────.       │
    ///  │   Article Cache  │─────────▶  (MyKit/MyClass)──────┘
    ///  └──────────────────┘             `───────────'       │
    ///                                       Article         │
    ///  ┌──────────────────┐             .───────────.       │
    ///  │   Symbol graph   │─────────▶  (MyKit/MyClass)──────┘
    ///  └──────────────────┘             `───────────'
    ///                                       Symbol
    /// ```
    private func addSymbolsToTopicGraph(symbolGraph: UnifiedSymbolGraph, url: URL?, symbolReferences: [SymbolGraph.Symbol.Identifier: [ResolvedTopicReference]], moduleReference: ResolvedTopicReference, bundle: DocumentationBundle) {
        let symbols = Array(symbolGraph.symbols.values)
        let results: [(AddSymbolResultWithProblems, [ResolvedTopicReference])] = symbols.concurrentPerform { symbol, results in
            if let selector = symbol.defaultSelector, let module = symbol.modules[selector] {
                guard let references = symbolReferences[symbol.defaultIdentifier], let reference = references.first else {
                    fatalError("Symbol with identifier '\(symbol.uniqueIdentifier)' has no reference. A symbol will always have at least one reference.")
                }
                
                let result = preparedSymbolData(
                    symbol,
                    reference: reference,
                    module: module,
                    moduleReference: moduleReference,
                    bundle: bundle,
                    fileURL: url
                )
                results.append((result, Array(references.dropFirst())))
            }
        }
        results.forEach { result, referenceAliases in
            addPreparedSymbolToContext(result)
            documentationCacheBasedLinkResolver.addPreparedSymbol(symbolReference: result.0.reference, referenceAliases: referenceAliases)
        }
    }

    /// Adds a prepared symbol data including a topic graph node and documentation node to the context.
    private func addPreparedSymbolToContext(_ result: AddSymbolResultWithProblems) {
        let symbolData = result.0
        topicGraph.addNode(symbolData.topicGraphNode)
        documentationCache[symbolData.reference] = symbolData.node
        symbolIndex[symbolData.preciseIdentifier] = symbolData.node
        
        for anchor in result.0.node.anchorSections {
            nodeAnchorSections[anchor.reference] = anchor
        }
        
        diagnosticEngine.emit(result.problems)
    }
    
    /// Loads all graph files from a given `bundle` and merges them together while building the symbol relationships and loading any available markdown documentation for those symbols.
    ///
    /// - Parameter bundle: The bundle to load symbol graph files from.
    /// - Returns: A pair of the references to all loaded modules and the hierarchy of all the loaded symbol's references.
    private func registerSymbols(from bundle: DocumentationBundle, symbolGraphLoader: SymbolGraphLoader) throws -> (moduleReferences: Set<ResolvedTopicReference>, urlHierarchy: BidirectionalTree<ResolvedTopicReference>) {
        // Making sure that we correctly let decoding memory get released, do not remove the autorelease pool.
        return try autoreleasepool { [documentationCacheBasedLinkResolver] () -> (Set<ResolvedTopicReference>, BidirectionalTree<ResolvedTopicReference>) in
            /// A tree of the symbol hierarchy as defined by the combined symbol graph.
            var symbolsURLHierarchy = BidirectionalTree<ResolvedTopicReference>(root: bundle.documentationRootReference)
            /// We need only unique relationships so we'll collect them in a set.
            var combinedRelationships = [UnifiedSymbolGraph.Selector: Set<SymbolGraph.Relationship>]()
            /// Collect symbols from all symbol graphs.
            var combinedSymbols = [String: UnifiedSymbolGraph.Symbol]()
            
            var moduleReferences = [String: ResolvedTopicReference]()
            
            // Build references for all symbols in all of this module's symbol graphs.
            let symbolReferences: [SymbolGraph.Symbol.Identifier : [ResolvedTopicReference]]
            if LinkResolutionMigrationConfiguration.shouldUseHierarchyBasedLinkResolver {
                symbolReferences = hierarchyBasedLinkResolver!.referencesForSymbols(in: symbolGraphLoader.unifiedGraphs, bundle: bundle, context: self)
                    .mapValues({ [$0] }) // The documentation cache implementation uses an array of values to handle multi languages
            } else {
                symbolReferences = documentationCacheBasedLinkResolver.referencesForSymbols(in: symbolGraphLoader.unifiedGraphs, bundle: bundle, context: self)
            }
            
            // Set the index and cache storage capacity to avoid ad-hoc storage resizing.
            symbolIndex.reserveCapacity(symbolReferences.count)
            documentationCache.reserveCapacity(symbolReferences.count)
            documentLocationMap.reserveCapacity(symbolReferences.count)
            topicGraph.nodes.reserveCapacity(symbolReferences.count)
            topicGraph.edges.reserveCapacity(symbolReferences.count)
            symbolsURLHierarchy.reserveCapacity(symbolReferences.count)
            combinedRelationships.reserveCapacity(symbolReferences.count)
            combinedSymbols.reserveCapacity(symbolReferences.count)
            
            // Iterate over batches of symbol graphs, each batch describing one module.
            // Each batch contains one or more symbol graph files.
            for (moduleName, unifiedSymbolGraph) in symbolGraphLoader.unifiedGraphs {
                try shouldContinueRegistration()

                let fileURL = symbolGraphLoader.mainModuleURL(forModule: moduleName)
                
                let moduleInterfaceLanguages: Set<SourceLanguage>
                // FIXME: Update with new SymbolKit API once available.
                // This is a very inefficient way to gather the source languages
                // represented in a symbol graph. Adding a dedicated SymbolKit API is tracked
                // with github.com/apple/swift-docc-symbolkit/issues/32 and rdar://85982095.
                let symbolGraphLanguages = Set(
                    unifiedSymbolGraph.symbols.flatMap(\.value.sourceLanguages)
                )
                
                // If the symbol graph has no symbols, we cannot determine what languages is it available for,
                // so fall back to Swift.
                moduleInterfaceLanguages = symbolGraphLanguages.isEmpty ? [.swift] : symbolGraphLanguages
                
                // If it's an existing module, update the interface languages
                moduleReferences[moduleName] = moduleReferences[moduleName]?.addingSourceLanguages(moduleInterfaceLanguages)
                
                // Import the symbol graph symbols
                let moduleReference: ResolvedTopicReference
                
                // If it's a repeating module, diff & merge matching declarations.
                if let existingModuleReference = moduleReferences[moduleName] {
                    // This node is known to exist
                    moduleReference = existingModuleReference
                    
                    try mergeSymbolDeclarations(from: unifiedSymbolGraph, references: symbolReferences, moduleReference: moduleReference, bundle: bundle, fileURL: fileURL)
                } else {
                    guard symbolGraphLoader.hasPrimaryURL(moduleName: moduleName) else { continue }
                    
                    // Create a module symbol
                    let moduleIdentifier = SymbolGraph.Symbol.Identifier(
                        precise: moduleName,
                        interfaceLanguage: moduleInterfaceLanguages.first!.id
                    )
                    
                    // Use the default module kind for this bundle if one was provided,
                    // otherwise fall back to 'Framework'
                    let moduleKindDisplayName = bundle.info.defaultModuleKind ?? "Framework"
                    let moduleSymbol = SymbolGraph.Symbol(
                            identifier: moduleIdentifier,
                            names: SymbolGraph.Symbol.Names(title: moduleName, navigator: nil, subHeading: nil, prose: nil),
                            pathComponents: [moduleName],
                            docComment: nil,
                            accessLevel: SymbolGraph.Symbol.AccessControl(rawValue: "public"),
                            kind: SymbolGraph.Symbol.Kind(parsedIdentifier: .module, displayName: moduleKindDisplayName),
                            mixins: [:])
                    let moduleSymbolReference = SymbolReference(moduleName, interfaceLanguages: moduleInterfaceLanguages, defaultSymbol: moduleSymbol)
                    moduleReference = ResolvedTopicReference(symbolReference: moduleSymbolReference, moduleName: moduleName, bundle: bundle)
                    
                    addSymbolsToTopicGraph(symbolGraph: unifiedSymbolGraph, url: fileURL, symbolReferences: symbolReferences, moduleReference: moduleReference, bundle: bundle)
                    
                    // For inherited symbols we remove the source docs (if inheriting docs is disabled) before creating their documentation nodes.
                    for (_, relationships) in unifiedSymbolGraph.relationshipsByLanguage {
                        for relationship in relationships {
                            // Check for an origin key.
                            if relationship.mixins[SymbolGraph.Relationship.SourceOrigin.mixinKey] != nil
                                // Check if it's a memberOf or implementation relationship.
                                && (relationship.kind == .memberOf || relationship.kind == .defaultImplementationOf) {
                                
                                SymbolGraphRelationshipsBuilder.addInheritedDefaultImplementation(
                                    edge: relationship,
                                    context: self,
                                    symbolIndex: &symbolIndex,
                                    moduleName: moduleName,
                                    engine: diagnosticEngine
                                )
                            }
                        }
                    }
                    
                    if let rootURL = symbolGraphLoader.mainModuleURL(forModule: moduleName), let rootModule = unifiedSymbolGraph.moduleData[rootURL] {
                        addPreparedSymbolToContext(
                            preparedSymbolData(.init(fromSingleSymbol: moduleSymbol, module: rootModule, isMainGraph: true), reference: moduleReference, module: rootModule, moduleReference: moduleReference, bundle: bundle, fileURL: fileURL)
                        )
                    }

                    // Add this module to the dictionary of processed modules to keep track of repeat symbol graphs
                    moduleReferences[moduleName] = moduleReference
                    
                    if hierarchyBasedLinkResolver == nil || LinkResolutionMigrationConfiguration.shouldReportLinkResolutionMismatches {
                        // Add modules as root nodes in the URL tree
                        try symbolsURLHierarchy.add(moduleReference, parent: symbolsURLHierarchy.root)
                    }
                }
                
                if hierarchyBasedLinkResolver == nil || LinkResolutionMigrationConfiguration.shouldReportLinkResolutionMismatches {
                    // Map the symbol graph hierarchy into a URL tree to use as default curation
                    // for symbols that aren't manually curated via documentation extension.
                    
                    // Curate all root framework symbols under the module in the URL tree
                    for symbol in unifiedSymbolGraph.symbols.values where symbol.defaultSymbol!.pathComponents.count == 1 {
                        try symbolIndex[symbol.uniqueIdentifier].map({
                            // If merging symbol graph extension there can be repeat symbols, don't add them again.
                            guard (try? symbolsURLHierarchy.parent(of: $0.reference)) == nil else { return }
                            try symbolsURLHierarchy.add($0.reference, parent: moduleReference)
                        })
                    }
                }
                
                // Collect symbols and relationships
                combinedSymbols.merge(unifiedSymbolGraph.symbols, uniquingKeysWith: { $1 })
                
                for (selector, relationships) in unifiedSymbolGraph.relationshipsByLanguage {
                    combinedRelationships[selector, default: []].formUnion(relationships)
                }
                
                // Keep track of relationships that refer to symbols that are absent from the symbol graph, so that
                // we can diagnose them.
                combinedRelationships[
                    .init(interfaceLanguage: "unknown", platform: nil),
                    default: []
                ].formUnion(unifiedSymbolGraph.orphanRelationships)
            }
            
            try shouldContinueRegistration()
            
            // Only add the symbol mapping now if the path hierarchy based resolver is the main implementation.
            // If it is only used for mismatch checking then we must wait until the documentation cache code path has traversed and updated all the colliding nodes.
            // Otherwise the mappings will save the unmodified references and the hierarchy based resolver won't find the expected parent nodes when resolving links.
            if LinkResolutionMigrationConfiguration.shouldUseHierarchyBasedLinkResolver {
                hierarchyBasedLinkResolver!.addMappingForSymbols(symbolIndex: symbolIndex)
            }
            
            if hierarchyBasedLinkResolver == nil || LinkResolutionMigrationConfiguration.shouldReportLinkResolutionMismatches {
                // The `symbolsURLHierarchy` is only used in the cache-based link resolver to traverse the documentation and
                // update child references to also include their parent's disambiguation.
                //
                // It is also used to compute the would-be symbol paths when gathering symbol path mismatches between the two
                // link resolution implementations.
                for (_, relationships) in combinedRelationships {
                    // Add parent <-> child edges to the URL tree
                    try relationships
                        .compactMap(parentChildRelationship(from:))
                        .sorted(by: Self.sortRelationshipsPreOrder)
                        .forEach({ pair in
                            // Add the relationship to the URL hierarchy
                            let (parentRef, childRef) = pair
                            // If the unique reference already exists, it's been added by another symbol graph
                            // likely built for a different target platform, ignore it as it's the exact same symbol.
                            if (try? symbolsURLHierarchy.parent(of: childRef)) == nil {
                                do {
                                    try symbolsURLHierarchy.add(childRef, parent: parentRef)
                                } catch let error as BidirectionalTree<ResolvedTopicReference>.Error {
                                    switch error {
                                        // Some parents might not exist if they are types from other frameworks and
                                        // those are not pulled into and available in the current symbol graph.
                                    case .nodeNotFound: break
                                    default: throw error
                                    }
                                }
                            }
                        })
                }
                
                // Only update the nodes and symbol index when using the documentation cache-based link resolver otherwise the context will end up in an inconsistent state.
                if !LinkResolutionMigrationConfiguration.shouldUseHierarchyBasedLinkResolver {
                    // Update the children of collision URLs. Walk the tree and update any dependents of updated URLs
                    for moduleReference in moduleReferences.values {
                        try symbolsURLHierarchy.traversePreOrder(from: moduleReference) { reference in
                            try self.documentationCacheBasedLinkResolver.updateNodeWithReferenceIfCollisionChild(reference, symbolsURLHierarchy: &symbolsURLHierarchy, symbolIndex: &symbolIndex, context: self)
                        }
                    }
                    
                    // Now that the colliding references have been updated, the hierarchy based resolver can save the reference to identifier mapping.
                    hierarchyBasedLinkResolver?.addMappingForSymbols(symbolIndex: symbolIndex)
                }
                
                if LinkResolutionMigrationConfiguration.shouldReportLinkResolutionPathMismatches {
                    // The way that symbol path mismatches are gathered depend on which link resolution implementation is used to resolve links.
                    if LinkResolutionMigrationConfiguration.shouldUseHierarchyBasedLinkResolver {
                        // Attempting to use the The documentation cache based link resolver to compute the disambiguated symbol paths when it is not in full control of the symbol index,
                        // topic graph, and documentation cache will result in the wrong behavior. The issues range from incorrectly computed paths to precondition failures.
                        //
                        // To compute what the disambiguated symbol paths would be for the documentation cache based link resolver, one needs to first compute the initial disambiguated
                        // paths and then traverse them to update the child paths to match their parents disambiguation prefix.
                        var cacheBasedReferences = documentationCacheBasedLinkResolver.referencesForSymbols(in: symbolGraphLoader.unifiedGraphs, bundle: bundle, context: self).mapValues({ $0.first! })
                        // Normally the second step would be done by traversing the `symbolsURLHierarchy` but since that is constructed using disambiguated references that didn't originate
                        // from the documentation cache based link resolver the `symbolsURLHierarchy` can't accurately map to the references that are computed by the documentation cache based link resolver.
                        //
                        // Luckily the path hierarchy describe the hierarchical relationships between the symbols in a way where it's possible to get the `Symbol.Identifier` for each symbol.
                        // By sorting the pairs of `(symbol, parent symbol)` by the symbol's number of path components it's possible to traverse the hierarchy breath first and update the initial disambiguated
                        // paths from the documentation cache based link resolver so that each child path matches the disambiguation from the parent path.
                        let sortedSymbolAndParentPairs = hierarchyBasedLinkResolver!.pathHierarchy.lookup.values.lazy
                            .filter { $0.symbol != nil && $0.parent?.symbol != nil }
                            .sorted(by: \.symbol!.pathComponents.count)
                            .map { ($0.symbol!.identifier, $0.parent!.symbol!.identifier) }
                        
                        for (symbolID, parentID) in sortedSymbolAndParentPairs {
                            if let symbolCacheReference = cacheBasedReferences[symbolID],
                               symbolCacheReference.pathComponents.count > 3,
                               let parentCacheReference = cacheBasedReferences[parentID],
                               parentCacheReference.pathComponents != symbolCacheReference.pathComponents.dropLast()
                            {
                                cacheBasedReferences[symbolID] = parentCacheReference.appendingPath(symbolCacheReference.lastPathComponent)
                            }
                        }
                        
                        let hierarchyBasedReferences = symbolReferences.mapValues({ $0.first! })
                        for (id, hierarchyBasedReference) in hierarchyBasedReferences {
                            guard let cacheBasedMainReference = cacheBasedReferences[id] else {
                                linkResolutionMismatches.missingPathsInCacheBasedLinkResolver.append(hierarchyBasedReference.path)
                                continue
                            }
                            if hierarchyBasedReference.path != cacheBasedMainReference.path {
                                linkResolutionMismatches.pathsWithMismatchedDisambiguation[hierarchyBasedReference.path] = cacheBasedMainReference.path
                            }
                        }
                        for (id, cacheBasedReference) in cacheBasedReferences where !hierarchyBasedReferences.keys.contains(id) && symbolGraphLoader.hasPrimaryURL(moduleName: cacheBasedReference.pathComponents[2]) {
                            linkResolutionMismatches.missingPathsInHierarchyBasedLinkResolver.append(cacheBasedReference.path)
                        }
                    } else {
                        // If the documentation cache based implementation is used then it has already fully updated the symbol index and the documentation cache by this point.
                        let hierarchyBasedReferences = hierarchyBasedLinkResolver!.referencesForSymbols(in: symbolGraphLoader.unifiedGraphs, bundle: bundle, context: self)
                        
                        for (usr, hierarchyBasedReference) in hierarchyBasedReferences {
                            guard let cacheBasedMainReference = symbolIndex[usr.precise]?.reference.path else {
                                linkResolutionMismatches.missingPathsInCacheBasedLinkResolver.append(hierarchyBasedReference.path)
                                continue
                            }
                            
                            if hierarchyBasedReference.path != cacheBasedMainReference {
                                linkResolutionMismatches.pathsWithMismatchedDisambiguation[hierarchyBasedReference.path] = cacheBasedMainReference
                            }
                        }
                        for (usr, node) in symbolIndex {
                            guard let kind = node.symbol?.kind.identifier, kind.symbolGeneratesPage(),
                                  !hierarchyBasedReferences.keys.contains(where: { $0.precise == usr })
                            else { continue }
                            linkResolutionMismatches.missingPathsInHierarchyBasedLinkResolver.append(node.reference.path)
                        }
                    }
                }
            }
            
            // Create inherited API collections
            try GeneratedDocumentationTopics.createInheritedSymbolsAPICollections(
                relationships: combinedRelationships.flatMap(\.value),
                context: self,
                bundle: bundle
            )

            // Parse and prepare the nodes' content concurrently.
            let updatedNodes: [(node: DocumentationNode, matchedArticleURL: URL?)] = Array(symbolIndex.values)
                .concurrentPerform { node, results in
                    let finalReference = node.reference
                    
                    // Match the symbol's documentation extension and initialize the node content.
                    let matches = uncuratedDocumentationExtensions[finalReference]
                    let updatedNode = nodeWithInitializedContent(reference: finalReference, matches: matches)
                    
                    results.append((
                        node: updatedNode,
                        matchedArticleURL: matches?.first?.source
                    ))
                }
            
            // Update cache with up-to-date nodes
            for (updatedNode, matchedArticleURL) in updatedNodes {
                let reference = updatedNode.reference
                // Add node's anchors to index
                for anchor in updatedNode.anchorSections {
                    nodeAnchorSections[anchor.reference] = anchor
                }
                // Update cache and lookup indexes with the updated node value
                documentationCache[reference] = updatedNode
                if let symbol = documentationCache[reference]!.symbol {
                    symbolIndex[symbol.identifier.precise] = documentationCache[reference]!
                }
                if let url = matchedArticleURL {
                    documentLocationMap[url] = reference
                }
                // Remove the matched article
                uncuratedDocumentationExtensions.removeValue(forKey: reference)
            }

            // Resolve any external references first
            try preResolveExternalLinks(references: Array(moduleReferences.values) + combinedSymbols.keys.compactMap({ symbolIndex[$0]?.reference }), bundle: bundle)
            
            // Look up and add symbols that are _referenced_ in the symbol graph but don't exist in the symbol graph.
            try resolveExternalSymbols(in: combinedSymbols, relationships: combinedRelationships)
            
            for (selector, relationships) in combinedRelationships {
                // Build relationships in the completed graph
                buildRelationships(relationships, selector: selector, bundle: bundle, engine: diagnosticEngine)
            }
            
            // Index references
            if !LinkResolutionMigrationConfiguration.shouldUseHierarchyBasedLinkResolver {
                documentationCacheBasedLinkResolver.referencesIndex.removeAll()
                documentationCacheBasedLinkResolver.referencesIndex.reserveCapacity(knownIdentifiers.count)
                for reference in knownIdentifiers {
                    documentationCacheBasedLinkResolver.registerReference(reference)
                }
            }

            return (moduleReferences: Set(moduleReferences.values), urlHierarchy: symbolsURLHierarchy)
        }
    }

    private func shouldContinueRegistration() throws {
        guard isRegistrationEnabled.sync({ $0 }) else {
            throw ContextError.registrationDisabled
        }
    }
    
    /// Builds in-memory relationships between symbols based on the relationship information in a given symbol graph file.
    ///
    /// - Parameters:
    ///   - symbolGraph: The symbol graph whose symbols to add in-memory relationships to.
    ///   - bundle: The bundle that the symbols belong to.
    ///   - problems: A mutable collection of problems to update with any problem encountered while building symbol relationships.
    ///
    /// ## See Also
    /// - ``SymbolGraphRelationshipsBuilder``
    func buildRelationships(
        _ relationships: Set<SymbolGraph.Relationship>,
        selector: UnifiedSymbolGraph.Selector,
        bundle: DocumentationBundle,
        engine: DiagnosticEngine
    ) {
        for edge in relationships {
            switch edge.kind {
            case .conformsTo:
                // Build conformant type <-> protocol relationships
                SymbolGraphRelationshipsBuilder.addConformanceRelationship(
                    edge: edge,
                    selector: selector,
                    in: bundle,
                    symbolIndex: &symbolIndex,
                    engine: diagnosticEngine
                )
            case .defaultImplementationOf:
                // Build implementation <-> protocol requirement relationships.
                SymbolGraphRelationshipsBuilder.addImplementationRelationship(
                    edge: edge,
                    selector: selector,
                    in: bundle,
                    context: self,
                    symbolIndex: &symbolIndex, engine: diagnosticEngine
                )
            case .inheritsFrom:
                // Build ancestor <-> offspring relationships.
                SymbolGraphRelationshipsBuilder.addInheritanceRelationship(
                    edge: edge,
                    selector: selector,
                    in: bundle,
                    symbolIndex: &symbolIndex,
                    engine: diagnosticEngine
                )
            case .requirementOf:
                // Build required member -> protocol relationships.
                SymbolGraphRelationshipsBuilder.addRequirementRelationship(
                    edge: edge,
                    selector: selector,
                    in: bundle,
                    symbolIndex: &symbolIndex,
                    engine: diagnosticEngine
                )
            case .optionalRequirementOf:
                // Build optional required member -> protocol relationships.
                SymbolGraphRelationshipsBuilder.addOptionalRequirementRelationship(
                    edge: edge,
                    selector: selector,
                    in: bundle,
                    symbolIndex: &symbolIndex,
                    engine: diagnosticEngine
                )
            default:
                break
            }
        }
    }
    
    /// Look up and add symbols that are _referenced_ in the symbol graph but don't exist in the symbol graph, using an `externalSymbolResolver` (if not `nil`).
    func resolveExternalSymbols(
        in symbols: [String: UnifiedSymbolGraph.Symbol],
        relationships: [UnifiedSymbolGraph.Selector: Set<SymbolGraph.Relationship>]
    ) throws {
        guard let symbolResolver = externalSymbolResolver else {
            return
        }
        
        // Gather all the references to symbols that don't exist in the combined symbol graph file and add then by resolving these "external" symbols.
        var symbolsToResolve = Set<String>()
        
        // Add all the symbols that are the target of a relationship. These could for example be protocols that are being conformed to,
        // classes that are being subclassed, or methods that are being overridden.
        for (_, relationships) in relationships {
            for edge in relationships where symbolIndex[edge.target] == nil {
                symbolsToResolve.insert(edge.target)
            }
        }
        
        // Add all the types that are referenced in a declaration. These could for example be the type of an argument or return value.
        for symbol in symbols.values {
            guard let defaultSymbol = symbol.defaultSymbol, let declaration = defaultSymbol.mixins[SymbolGraph.Symbol.DeclarationFragments.mixinKey] as? SymbolGraph.Symbol.DeclarationFragments else {
                continue
            }
            for fragment in declaration.declarationFragments {
                guard let preciseIdentifier = fragment.preciseIdentifier, symbolIndex[preciseIdentifier] == nil else {
                    continue
                }
                symbolsToResolve.insert(preciseIdentifier)
            }
        }
        
        // TODO: When the symbol graph includes the precise identifiers for conditional availability, those symbols should also be resolved (rdar://63768609).
        
        // Resolve all the collected symbol identifiers and add them do the topic graph.
        for symbolIdentifier in symbolsToResolve {
            do {
                let symbolNode = try symbolResolver.symbolEntity(withPreciseIdentifier: symbolIdentifier)
                symbolIndex[symbolIdentifier] = symbolNode
                
                // Keep track of which symbols were added to the topic graph from external sources so that their pages are not rendered.
                externallyResolvedSymbols.insert(symbolNode.reference)
                
                documentationCache[symbolNode.reference] = symbolNode
            } catch {
                diagnosticEngine.emit(Problem(diagnostic: Diagnostic(source: nil, severity: .warning, range: nil, identifier: "org.swift.docc.ReferenceSymbolNotFound", summary: "Symbol with identifier \(symbolIdentifier.singleQuoted) was referenced in the combined symbol graph but couldn't be found in the symbol graph or externally: \(error.localizedDescription)"), possibleSolutions: []))
            }
        }
    }
    
    /// When building multi-platform documentation symbols might have more than one declaration
    /// depending on variances in their implementation across platforms (e.g. use `NSPoint` vs `CGPoint` parameter in a method).
    /// This method finds matching symbols between graphs and merges their declarations in case there are differences.
    func mergeSymbolDeclarations(from otherSymbolGraph: UnifiedSymbolGraph, references: [SymbolGraph.Symbol.Identifier: [ResolvedTopicReference]], moduleReference: ResolvedTopicReference, bundle: DocumentationBundle, fileURL otherSymbolGraphURL: URL?) throws {
        let mergeError = Synchronized<Error?>(nil)
        
        let results: [(AddSymbolResultWithProblems, [ResolvedTopicReference])] = Array(otherSymbolGraph.symbols.values).concurrentPerform { symbol, result in
            guard let defaultSymbol = symbol.defaultSymbol, let swiftSelector = symbol.defaultSelector, let module = symbol.modules[swiftSelector] else {
                fatalError("""
                    Only Swift symbols are currently supported. \
                    This initializer is only called with symbols from the symbol graph, which currently only supports Swift.
                    """
                )
            }
            guard (defaultSymbol.mixins[SymbolGraph.Symbol.DeclarationFragments.mixinKey] as? SymbolGraph.Symbol.DeclarationFragments) != nil else {
                diagnosticEngine.emit(Problem(diagnostic: Diagnostic(source: nil, severity: .error, range: nil, identifier: "org.swift.docc.SymbolDeclarationNotFound", summary: "Symbol with identifier '\(symbol.uniqueIdentifier)' has no declaration"), possibleSolutions: []))
                return
            }
            
            guard let existingNode = symbolIndex[symbol.uniqueIdentifier], existingNode.semantic is Symbol else {
                // New symbols that didn't exist in the previous graphs should be added.
                guard let references = references[symbol.defaultIdentifier], let reference = references.first else {
                    fatalError("Symbol with identifier '\(symbol.uniqueIdentifier)' has no reference. A symbol will always have at least one reference.")
                }
                
                result.append(
                    (preparedSymbolData(symbol, reference: reference, module: module, moduleReference: moduleReference, bundle: bundle, fileURL: otherSymbolGraphURL), Array(references.dropFirst()))
                )
                    return
            }
            
            do {
                // It's safe to force unwrap since we validated the data above.
                // We update the node in place so avoid copying the data around.
                try (existingNode.semantic as! Symbol).mergeDeclarations(unifiedSymbol: symbol)
            } catch {
                // Invalid input data, throw the error.
                mergeError.sync({
                    if $0 == nil { $0 = error }
                })
            }
        }
        
        // If there was an invalid input error re-throw it.
        if let error = mergeError.sync({ $0 }) {
            throw error
        }

        // Add any new symbols to the documentation cache.
        results.forEach { result, referenceAliases in
            addPreparedSymbolToContext(result)
            documentationCacheBasedLinkResolver.addPreparedSymbol(symbolReference: result.0.reference, referenceAliases: referenceAliases)
        }
    }
    
    private static let supportedImageExtensions: Set<String> = ["png", "jpg", "jpeg", "svg", "gif"]
    private static let supportedVideoExtensions: Set<String> = ["mov", "mp4"]

    // TODO: Move this functionality to ``DocumentationBundleFileTypes`` (rdar://68156425).
    
    /// A type of asset.
    public enum AssetType: CustomStringConvertible {
        /// An image asset.
        case image
        /// A video asset.
        case video
        
        public var description: String {
            switch self {
            case .image:
                return "Image"
            case .video:
                return "Video"
            }
        }
    }

    /// Checks if a given `fileExtension` is supported as a `type` of asset.
    ///
    /// - Parameters:
    ///   - fileExtension: The file extension to check.
    ///   - type: The type of asset to check if the `fileExtension` is supported for.
    /// - Returns: Whether or not the file extension is supported for the given type of asset.
    public static func isFileExtension(_ fileExtension: String, supported type: AssetType) -> Bool {
        let fileExtension = fileExtension.lowercased()
        switch type {
            case .image: return supportedImageExtensions.contains(fileExtension)
            case .video: return supportedVideoExtensions.contains(fileExtension)
        }
    }
    
    private func registerMiscResources(from bundle: DocumentationBundle) throws {
        let miscResources = Set(bundle.miscResourceURLs)
        try assetManagers[bundle.identifier, default: DataAssetManager()]
            .register(data: miscResources, dataProvider: dataProvider, bundle: bundle)
    }
    
    private func registeredAssets(withExtensions extensions: Set<String>? = nil, inContexts contexts: [DataAsset.Context] = DataAsset.Context.allCases, forBundleID bundleIdentifier: BundleIdentifier) -> [DataAsset] {
        guard let resources = assetManagers[bundleIdentifier]?.storage.values else {
            return []
        }
        return resources.filter { dataAsset in
            // Filter by file extension.
            if let extensions = extensions {
                let fileExtensions = dataAsset.variants.values.map { $0.pathExtension.lowercased() }
                guard !extensions.intersection(fileExtensions).isEmpty else {
                    return false
                }
            }
            // Filter by context.
            return contexts.contains(dataAsset.context)
        }
    }

    /// Returns a list of all the image assets that registered for a given `bundleIdentifier`.
    ///
    /// - Parameter bundleIdentifier: The identifier of the bundle to return image assets for.
    /// - Returns: A list of all the image assets for the given bundle.
    public func registeredImageAssets(forBundleID bundleIdentifier: BundleIdentifier) -> [DataAsset] {
        return registeredAssets(withExtensions: DocumentationContext.supportedImageExtensions, forBundleID: bundleIdentifier)
    }
    
    /// Returns a list of all the video assets that registered for a given `bundleIdentifier`.
    ///
    /// - Parameter bundleIdentifier: The identifier of the bundle to return video assets for.
    /// - Returns: A list of all the video assets for the given bundle.
    public func registeredVideoAssets(forBundleID bundleIdentifier: BundleIdentifier) -> [DataAsset] {
        return registeredAssets(withExtensions: DocumentationContext.supportedVideoExtensions, forBundleID: bundleIdentifier)
    }

    /// Returns a list of all the download assets that registered for a given `bundleIdentifier`.
    ///
    /// - Parameter bundleIdentifier: The identifier of the bundle to return download assets for.
    /// - Returns: A list of all the download assets for the given bundle.
    public func registeredDownloadsAssets(forBundleID bundleIdentifier: BundleIdentifier) -> [DataAsset] {
        return registeredAssets(inContexts: [DataAsset.Context.download], forBundleID: bundleIdentifier)
    }

    typealias Articles = [DocumentationContext.SemanticResult<Article>]
    private typealias ArticlesTuple = (articles: Articles, rootPageArticles: Articles)

    private func splitArticles(_ articles: Articles) -> ArticlesTuple {
        return articles.reduce(into: ArticlesTuple(articles: [], rootPageArticles: [])) { result, article in
            if article.value.metadata?.technologyRoot != nil {
                result.rootPageArticles.append(article)
            } else {
                result.articles.append(article)
            }
        }
    }
    
    private func registerRootPages(from articles: Articles, in bundle: DocumentationBundle) -> Articles {
        // Create a root leaf node for all root page articles
        for article in articles {
            // Create the documentation data
            guard let (documentation, title) = DocumentationContext.documentationNodeAndTitle(for: article, kind: .collection, in: bundle) else { continue }
            let reference = documentation.reference
            
            // Create the documentation node
            documentLocationMap[article.source] = reference
            let topicGraphKind = DocumentationNode.Kind.module
            let graphNode = TopicGraph.Node(reference: reference, kind: topicGraphKind, source: .file(url: article.source), title: title)
            topicGraph.addNode(graphNode)
            documentationCache[reference] = documentation
            
            if let hierarchyBasedLinkResolver = hierarchyBasedLinkResolver {
                hierarchyBasedLinkResolver.addRootArticle(article, anchorSections: documentation.anchorSections)
            }
            for anchor in documentation.anchorSections {
                nodeAnchorSections[anchor.reference] = anchor
            }
            
            // Remove the article from the context
            uncuratedArticles.removeValue(forKey: article.topicGraphNode.reference)
        }
        return articles
    }
    
    /// When `true` bundle registration will be cancelled asap.
    private var isRegistrationEnabled = Synchronized<Bool>(true)
    
    /// Enables or disables bundle registration.
    ///
    /// When given `false` the context will try to cancel as quick as possible
    /// any ongoing bundle registrations.
    public func setRegistrationEnabled(_ value: Bool) {
        isRegistrationEnabled.sync({ $0 = value })
    }
    
    /// Adds articles that are not root pages to the documentation cache.
    ///
    /// This method adds all of the `articles` to the documentation cache and inserts a node representing
    /// the article into the topic graph.
    ///
    /// > Important: `articles` must not be root pages.
    ///
    /// - Parameters:
    ///   - articles: Articles to register with the documentation cache.
    ///   - bundle: The bundle containing the articles.
    /// - Returns: The articles that were registered, with their topic graph node updated to what's been added to the topic graph.
    private func registerArticles(
        _ articles: DocumentationContext.Articles,
        in bundle: DocumentationBundle
    ) -> DocumentationContext.Articles {
        articles.map { article in
            guard let (documentation, title) = DocumentationContext.documentationNodeAndTitle(
                for: article,
                
                // Articles are available in the same languages the only root module is available in. If there is more
                // than one module, we cannot determine what languages it's available in and default to Swift.
                availableSourceLanguages: soleRootModuleReference.map { sourceLanguages(for: $0) },
                kind: .article,
                in: bundle
            ) else {
                return article
            }
            let reference = documentation.reference
            
            documentationCache[reference] = documentation
            
            documentLocationMap[article.source] = reference
            let graphNode = TopicGraph.Node(reference: reference, kind: .article, source: .file(url: article.source), title: title)
            topicGraph.addNode(graphNode)
            
            if let hierarchyBasedLinkResolver = hierarchyBasedLinkResolver {
                hierarchyBasedLinkResolver.addArticle(article, anchorSections: documentation.anchorSections)
            }
            for anchor in documentation.anchorSections {
                nodeAnchorSections[anchor.reference] = anchor
            }
            
            var article = article
            // Update the article's topic graph node with the one we just added to the topic graph.
            article.topicGraphNode = graphNode
            return article
        }
    }
    
    /// Creates a documentation node and title for the given article semantic result.
    ///
    /// - Parameters:
    ///   - article: The article that will be used to create the returned documentation node.
    ///   - kind: The kind that should be used to create the returned documentation node.
    ///   - bundle: The documentation bundle this article belongs to.
    /// - Returns: A documentation node and title for the given article semantic result.
    static func documentationNodeAndTitle(
        for article: DocumentationContext.SemanticResult<Article>,
        availableSourceLanguages: Set<SourceLanguage>? = nil,
        kind: DocumentationNode.Kind,
        in bundle: DocumentationBundle
    ) -> (node: DocumentationNode, title: String)? {
        guard let articleMarkup = article.value.markup else {
            return nil
        }
        
        let path = NodeURLGenerator.pathForSemantic(article.value, source: article.source, bundle: bundle)
        
        // If available source languages are provided and it contains Swift, use Swift as the default language of
        // the article.
        let defaultSourceLanguage = defaultLanguage(in: availableSourceLanguages)
        
        let reference = ResolvedTopicReference(
            bundleIdentifier: bundle.identifier,
            path: path,
            sourceLanguages: availableSourceLanguages
                // FIXME: Pages in article-only catalogs should not be inferred as "Swift" as a fallback
                // (github.com/apple/swift-docc/issues/240).
                ?? [.swift]
        )
        
        let title = article.topicGraphNode.title
        
        let documentationNode = DocumentationNode(
            reference: reference,
            kind: kind,
            sourceLanguage: defaultSourceLanguage,
            availableSourceLanguages: availableSourceLanguages,
            name: .conceptual(title: title),
            markup: articleMarkup,
            semantic: article.value
        )
        
        return (documentationNode, title)
    }
    
    /// Curates articles under the root module.
    ///
    /// This method creates a new task group under the root page containing references to all of the articles
    /// in the bundle that haven't been manually curated.
    ///
    /// - Parameters:
    ///   - otherArticles: Non-root articles to curate.
    ///   - bundle: The bundle that contains the articles.
    ///   - rootNode: The node that will serve as the source of any topic graph edges created by this method.
    /// - Throws: If looking up a `DocumentationNode` for the root module reference fails.
    /// - Returns: An array of resolved references to the articles that were automatically curated.
    private func autoCurateArticles(_ otherArticles: DocumentationContext.Articles, in bundle: DocumentationBundle, startingFrom rootNode: TopicGraph.Node) throws -> [ResolvedTopicReference] {
        
        let autoCuratedArticles: DocumentationContext.Articles = otherArticles.compactMap { article in
            let edges = topicGraph.edges[article.topicGraphNode.reference] ?? []
            let reverseEdges = topicGraph.reverseEdges[article.topicGraphNode.reference] ?? []
            guard edges.isEmpty, reverseEdges.isEmpty else {
                return nil
            }

            topicGraph.addEdge(from: rootNode, to: article.topicGraphNode)
            uncuratedArticles.removeValue(forKey: article.topicGraphNode.reference)
            
            return article
        }
        
        guard !autoCuratedArticles.isEmpty else {
            return []
        }
        
        let articleReferences = autoCuratedArticles.map(\.topicGraphNode.reference)
        
        func createAutomaticTaskGroupSection(references: [ResolvedTopicReference]) -> AutomaticTaskGroupSection {
            AutomaticTaskGroupSection(
                title: "Articles",
                references: references,
                renderPositionPreference: .top
            )
        }
        
        let node = try entity(with: rootNode.reference)
        
        // If the node we're automatically curating the article under is a symbol, automatically curate the article
        // for each language it's available in.
        if let symbol = node.semantic as? Symbol {
            for sourceLanguage in node.availableSourceLanguages {
                symbol.automaticTaskGroupsVariants[
                    .init(interfaceLanguage: sourceLanguage.id)
                ] = [createAutomaticTaskGroupSection(references: articleReferences)]
            }
        } else if var taskGroupProviding = node.semantic as? AutomaticTaskGroupsProviding {
            taskGroupProviding.automaticTaskGroups = [
                createAutomaticTaskGroupSection(references: articleReferences)
            ]
        }
        
        return articleReferences
    }
    
    /**
     Register a documentation bundle with this context.
     */
    private func register(_ bundle: DocumentationBundle) throws {
        try shouldContinueRegistration()
        
        // Note: Each bundle is registered and processed separately.
        // Documents and symbols may both reference each other so the bundle is registered in 4 steps
        
        // In the bundle discovery phase all tasks run in parallel as they don't depend on each other.
        let discoveryGroup = DispatchGroup()
        let discoveryQueue = DispatchQueue(label: "org.swift.docc.Discovery", qos: .unspecified, attributes: .concurrent, autoreleaseFrequency: .workItem)
        
        let discoveryError = Synchronized<Error?>(nil)

        // Load all bundle symbol graphs into the loader.
        var symbolGraphLoader: SymbolGraphLoader!
        var hierarchyBasedResolver: PathHierarchyBasedLinkResolver!
        
        discoveryGroup.async(queue: discoveryQueue) { [unowned self] in
            symbolGraphLoader = SymbolGraphLoader(bundle: bundle, dataProvider: self.dataProvider)
            do {
                try symbolGraphLoader.loadAll(using: decoder)
                if LinkResolutionMigrationConfiguration.shouldSetUpHierarchyBasedLinkResolver {
                    let pathHierarchy = PathHierarchy(symbolGraphLoader: symbolGraphLoader, bundleName: urlReadablePath(bundle.displayName), knownDisambiguatedPathComponents: knownDisambiguatedSymbolPathComponents)
                    hierarchyBasedResolver = PathHierarchyBasedLinkResolver(pathHierarchy: pathHierarchy)
                }
            } catch {
                // Pipe the error out of the dispatch queue.
                discoveryError.sync({
                    if $0 == nil { $0 = error }
                })
            }
        }

        // First, all the resources are added since they don't reference anything else.
        discoveryGroup.async(queue: discoveryQueue) { [unowned self] in
            do {
                try self.registerMiscResources(from: bundle)
            } catch {
                // Pipe the error out of the dispatch queue.
                discoveryError.sync({
                    if $0 == nil { $0 = error }
                })
            }
        }
        
        // Second, all the documents and symbols are added.
        //
        // Note: Documents and symbols may look up resources at this point but shouldn't lookup other documents or
        //       symbols or attempt to resolve links/references since the topic graph may not contain all documents
        //       or all symbols yet.
        var result: (
            technologies: [SemanticResult<Technology>],
            tutorials: [SemanticResult<Tutorial>],
            tutorialArticles: [SemanticResult<TutorialArticle>],
            articles: [SemanticResult<Article>]
        )!
        
        discoveryGroup.async(queue: discoveryQueue) { [unowned self] in
            do {
                result = try self.registerDocuments(from: bundle)
            } catch {
                // Pipe the error out of the dispatch queue.
                discoveryError.sync({
                    if $0 == nil { $0 = error }
                })
            }
        }
        
        discoveryGroup.wait()

        try shouldContinueRegistration()

        // Re-throw discovery errors
        if let encounteredError = discoveryError.sync({ $0 }) {
            throw encounteredError
        }
        
        // All discovery went well, process the inputs.
        let (technologies, tutorials, tutorialArticles, allArticles) = result
        var (otherArticles, rootPageArticles) = splitArticles(allArticles)
        
        let globalOptions = (allArticles + uncuratedDocumentationExtensions.values.flatMap { $0 }).compactMap { article in
            return article.value.options[.global]
        }
        
        if globalOptions.count > 1 {
            let extraGlobalOptionsProblems = globalOptions.map { extraOptionsDirective -> Problem in
                let diagnostic = Diagnostic(
                    source: extraOptionsDirective.originalMarkup.nameLocation?.source,
                    severity: .warning,
                    range: extraOptionsDirective.originalMarkup.range,
                    identifier: "org.swift.docc.DuplicateGlobalOptions",
                    summary: "Duplicate \(extraOptionsDirective.scope) \(Options.directiveName.singleQuoted) directive",
                    explanation: """
                    A DocC catalog can only contain a single \(Options.directiveName.singleQuoted) \
                    directive with the \(extraOptionsDirective.scope.rawValue.singleQuoted) scope.
                    """
                )
                
                guard let range = extraOptionsDirective.originalMarkup.range else {
                    return Problem(diagnostic: diagnostic)
                }
                
                let solution = Solution(
                    summary: "Remove extraneous \(extraOptionsDirective.scope) \(Options.directiveName.singleQuoted) directive",
                    replacements: [
                        Replacement(range: range, replacement: "")
                    ]
                )
                
                return Problem(diagnostic: diagnostic, possibleSolutions: [solution])
            }
            
            diagnosticEngine.emit(extraGlobalOptionsProblems)
        } else {
            options = globalOptions.first
        }
        
        if LinkResolutionMigrationConfiguration.shouldSetUpHierarchyBasedLinkResolver {
            hierarchyBasedLinkResolver = hierarchyBasedResolver
            hierarchyBasedResolver.addMappingForRoots(bundle: bundle)
            for tutorial in tutorials {
                hierarchyBasedResolver.addTutorial(tutorial)
            }
            for article in tutorialArticles {
                hierarchyBasedResolver.addTutorialArticle(article)
            }
            for technology in technologies {
                hierarchyBasedResolver.addTechnology(technology)
            }
        }
        
        let rootPages = registerRootPages(from: rootPageArticles, in: bundle)
        let (moduleReferences, symbolsURLHierarchy) = try registerSymbols(from: bundle, symbolGraphLoader: symbolGraphLoader)
        // We don't need to keep the loader in memory after we've registered all symbols.
        symbolGraphLoader = nil
        
        try shouldContinueRegistration()
        
        // Keep track of the root modules registered from symbol graph files, we'll need them to automatically
        // curate articles.
        rootModules = topicGraph.nodes.values.compactMap { node in
            guard node.kind == .module else {
                return nil
            }
            return node.reference
        }
        
        // Articles that will be automatically curated can be resolved but they need to be pre registered before resolving links.
        let rootNodeForAutomaticCuration = soleRootModuleReference.flatMap(topicGraph.nodeWithReference(_:))
        if rootNodeForAutomaticCuration != nil {
            otherArticles = registerArticles(otherArticles, in: bundle)
            try shouldContinueRegistration()
        }
        
        // Third, any processing that relies on resolving other content is done, mainly resolving links.
        try preResolveExternalLinks(semanticObjects:
            technologies.map(referencedSemanticObject) +
            tutorials.map(referencedSemanticObject) +
            tutorialArticles.map(referencedSemanticObject),
            bundle: bundle)
        
        resolveLinks(
            technologies: technologies,
            tutorials: tutorials,
            tutorialArticles: tutorialArticles,
            bundle: bundle
        )
        
        try shouldContinueRegistration()
        var allCuratedReferences: Set<ResolvedTopicReference>
        if LinkResolutionMigrationConfiguration.shouldUseHierarchyBasedLinkResolver {
            let topLevelModuleReferences = hierarchyBasedLinkResolver!.topLevelSymbols()
            allCuratedReferences = try crawlSymbolCuration(in: topLevelModuleReferences, bundle: bundle)
        } else {
            allCuratedReferences = try crawlSymbolCuration(rootModules: moduleReferences, rootPages: rootPages, symbolsURLHierarchy: symbolsURLHierarchy, bundle: bundle)
        }
        
        // Store the list of manually curated references if doc coverage is on.
        if shouldStoreManuallyCuratedReferences {
            manuallyCuratedReferences = allCuratedReferences
        }
        
        try shouldContinueRegistration()

        // Fourth, automatically curate all symbols that haven't been curated manually
        let automaticallyCurated = autoCurateSymbolsInTopicGraph(symbolsURLHierarchy: symbolsURLHierarchy, engine: diagnosticEngine)
        
        // Crawl the rest of the symbols that haven't been crawled so far in hierarchy pre-order.
        allCuratedReferences = try crawlSymbolCuration(in: automaticallyCurated.map(\.child), bundle: bundle, initial: allCuratedReferences)

        // Remove curation paths that have been created automatically above
        // but we've found manual curation for in the second crawl pass.
        removeUnneededAutomaticCuration(automaticallyCurated)
        
        // Automatically curate articles that haven't been manually curated
        // Article curation is only done automatically if there is only one root module
        if let rootNode = rootNodeForAutomaticCuration {
            let articleReferences = try autoCurateArticles(otherArticles, in: bundle, startingFrom: rootNode)
            try preResolveExternalLinks(references: articleReferences, bundle: bundle)
            resolveLinks(curatedReferences: Set(articleReferences), bundle: bundle)
        }

        // Emit warnings for any remaining uncurated files.
        emitWarningsForUncuratedTopics()
        
        if let hierarchyBasedLinkResolver = hierarchyBasedLinkResolver {
            hierarchyBasedLinkResolver.addAnchorForSymbols(symbolIndex: symbolIndex)
        }
        
        // Fifth, resolve links in nodes that are added solely via curation
        try preResolveExternalLinks(references: Array(allCuratedReferences), bundle: bundle)
        resolveLinks(curatedReferences: allCuratedReferences, bundle: bundle)

        // TODO: Calling `mergeExternalEntities` below ensures we have resolved all external entities during before we finish building the context.
        // We should use a read-only context during render time (rdar://65130130).

        // Sixth - fetch external entities and merge them in the context
        mergeExternalEntities(withReferences: Array(externallyResolvedSymbols))
        
        // Seventh, the complete topic graph—with all nodes and all edges added—is analyzed.
        topicGraphGlobalAnalysis()
        
        preResolveModuleNames()
    }
    
    /// Given a list of topics that have been automatically curated, checks if a topic has been additionally manually curated
    /// and if so removes the automatic curation.
    /// 
    /// During the first crawl pass we skip over all automatically curated nodes (as they are not in the topic graph yet.
    /// After adding all symbols automatically to their parents and running a second crawl pass we discover any manual
    /// curations that we could not crawl in the first pass.
    ///
    /// To remove the automatic curations that have been made "obsolete" via the second pass of crawling
    /// call `removeUnneededAutomaticCuration(_:)` which walks the list of automatic curations and removes
    /// the parent <-> child topic graph relationships that have been obsoleted.
    ///
    /// - Parameter automaticallyCurated: A list of topics that have been automatically curated.
    func removeUnneededAutomaticCuration(_ automaticallyCurated: [(child: ResolvedTopicReference, parent: ResolvedTopicReference)]) {
        for pair in automaticallyCurated {
            let paths = pathsTo(pair.child)
            
            // Collect all current unique parents of the child.
            let parents = Set(paths.map({ $0.last?.path }))
            
            // Check if the topic has multiple curation paths
            guard parents.count > 1 else { continue }
            
            // The topic has been manually curated, remove the automatic curation now.
            topicGraph.removeEdge(fromReference: pair.parent, toReference: pair.child)
        }
    }
    
    /// Resolves entities for the given external references and merges them into the documentation cache.
    /// - Note: We resolve external symbols serially as the communication channel
    /// with external resolvers might not be full-duplex.
    func mergeExternalEntities(withReferences references: [ResolvedTopicReference]) {
        for reference in references {
            // Try to resolve the reference if an external resolver exist for the reference's bundle identifier
            if let externalResolver = externalReferenceResolvers[reference.bundleIdentifier] {
                do {
                    documentationCache[reference] = try externalResolver.entity(with: reference)
                } catch {
                    diagnosticEngine.emit(Problem(diagnostic: Diagnostic(source: nil, severity: .warning, range: nil, identifier: "org.swift.docc.FailedToResolveExternalReference", summary: error.localizedDescription), possibleSolutions: []))
                }
            }
        }
    }
    
    /// Curate all remaining uncurated symbols under their natural parent from the symbol graph.
    ///
    /// This will include all symbols that were not manually curated by the documentation author.
    /// - Returns: An ordered list of symbol references that have been added to the topic graph automatically.
    private func autoCurateSymbolsInTopicGraph(symbolsURLHierarchy: BidirectionalTree<ResolvedTopicReference>, engine: DiagnosticEngine) -> [(child: ResolvedTopicReference, parent: ResolvedTopicReference)] {
        var automaticallyCuratedSymbols = [(ResolvedTopicReference, ResolvedTopicReference)]()
        if LinkResolutionMigrationConfiguration.shouldUseHierarchyBasedLinkResolver {
            hierarchyBasedLinkResolver!.traverseSymbolAndParentPairs { reference, parentReference in
                guard let topicGraphNode = topicGraph.nodeWithReference(reference),
                      let topicGraphParentNode = topicGraph.nodeWithReference(parentReference),
                      // Check that the node hasn't got any parents from manual curation
                      topicGraph.reverseEdges[reference] == nil
                else { return }
                topicGraph.addEdge(from: topicGraphParentNode, to: topicGraphNode)
                automaticallyCuratedSymbols.append((child: reference, parent: parentReference))
            }
            return automaticallyCuratedSymbols
        }
        
        do {
            // We'll collect references in an array to keep them pre-order in respect to their position in the symbol hierarchy
            // Walk all symbols and find their nodes in the topic graph
            try symbolsURLHierarchy.traversePreOrder { reference in
                
                // Skip over root node while traversing
                guard symbolsURLHierarchy.root != reference,
                    // Fetch the matching topic graph node
                    let topicGraphNode = topicGraph.nodeWithReference(reference),
                    // Check that the node hasn't got any parents from manual curation
                    topicGraph.reverseEdges[reference] == nil,
                    // Check that the symbol does have a parent in the symbol graph
                    let symbolGraphParentReference = try symbolsURLHierarchy.parent(of: reference),
                    // Fetch the topic graph node matching the symbol graph parent reference
                    let topicGraphParentNode = topicGraph.nodeWithReference(symbolGraphParentReference)
                    
                else { return }
                
                // Curate the symbol under its parent
                topicGraph.addEdge(from: topicGraphParentNode, to: topicGraphNode)
                automaticallyCuratedSymbols.append((child: reference, parent: symbolGraphParentReference))
            }
        } catch {
            // This will only ever happen if the symbol graph contains invalid relationships (cyclic links, a symbol with multiple parents, etc.)
            engine.emit(.init(diagnostic: Diagnostic(source: nil, severity: .error, range: nil, identifier: "org.swift.docc.InvalidSymbolGraphHierarchy", summary: error.localizedDescription), possibleSolutions: []))
        }
        
        return automaticallyCuratedSymbols
    }
    
    /// A closure type getting the information about a reference in a context and returns any possible problems with it.
    public typealias ReferenceCheck = (DocumentationContext, ResolvedTopicReference) -> [Problem]

    private var checks: [ReferenceCheck] = []
    
    /// Adds new checks to be run during the global topic analysis; after a bundle has been fully registered and its topic graph has been fully built.
    ///
    /// - Parameter newChecks: The new checks to add.
    public func addGlobalChecks(_ newChecks: [ReferenceCheck]) {
        checks.append(contentsOf: newChecks)
    }

    /// Crawls the hierarchy of symbol task groups from each module and root level page, adding relationships in the topic graph for all resolvable task group references.
    ///
    /// - Parameters:
    ///   - rootModules: The modules to crawl.
    ///   - rootPages: The other root level pages to crawl.
    ///   - bundle: The bundle to resolve symbol and article references against.
    /// - Returns: The references of all the symbols that were curated.
    ///
    /// ## See Also
    /// - ``DocumentationCurator``
    func crawlSymbolCuration<Modules: Sequence>(rootModules: Modules, rootPages: Articles, symbolsURLHierarchy: BidirectionalTree<ResolvedTopicReference>, bundle: DocumentationBundle) throws -> Set<ResolvedTopicReference> where Modules.Element == ResolvedTopicReference {
        // Start crawling at top-level symbols and decent the hierarchy following custom curations,
        // finally curate the top-level symbols under the module. We do this to account for the fact
        // top-level types aren't initially children of the module.
        let topLevelModuleReferences = try rootModules.flatMap(symbolsURLHierarchy.children(of:)) + rootModules
        
        // Crawl hierarchy under a root page for custom curations
        let rootPageReferences = rootPages.map(\.topicGraphNode.reference)
        return try crawlSymbolCuration(in: topLevelModuleReferences + rootPageReferences, bundle: bundle)
    }
    
    /// Crawls the hierarchy of the given list of nodes, adding relationships in the topic graph for all resolvable task group references.
    /// - Parameters:
    ///   - references: A list of references to crawl.
    ///   - bundle: A documentation bundle.
    ///   - initial: A list of references to skip when crawling.
    /// - Returns: The references of all the symbols that were curated.
    @discardableResult
    func crawlSymbolCuration(in references: [ResolvedTopicReference], bundle: DocumentationBundle, initial: Set<ResolvedTopicReference> = []) throws -> Set<ResolvedTopicReference> {
        var crawler = DocumentationCurator(in: self, bundle: bundle, initial: initial)

        for reference in references {
            try crawler.crawlChildren(
                of: reference,
                relateNodes: {
                    self.topicGraph.unsafelyAddEdge(source: $0, target: $1)
                }
            )
        }
        
        diagnosticEngine.emit(crawler.problems)
        
        return crawler.curatedNodes
    }

    /// Emits warnings for unmatched documentation extensions and uncurated articles.
    private func emitWarningsForUncuratedTopics() {
        // Check that all documentation extension files matched a symbol and that all articles are curated
        for results in uncuratedDocumentationExtensions.values {
            let articleResult = results.first!
            let remaining  = results.dropFirst()
            
            guard let link = articleResult.value.title?.child(at: 0) as? AnyLink else {
                fatalError("An article shouldn't have ended up in the documentation extension cache unless its title was a link. File: \(articleResult.source.absoluteString.singleQuoted)")
            }
            
            let notes: [DiagnosticNote] = remaining.map { articleResult in
                guard let linkMarkup = articleResult.value.title?.child(at: 0), linkMarkup is AnyLink else {
                    fatalError("An article shouldn't have ended up in the documentation extension cache unless its title was a link. File: \(articleResult.source.absoluteString.singleQuoted)")
                }
                let zeroRange = SourceLocation(line: 1, column: 1, source: nil)..<SourceLocation(line: 1, column: 1, source: nil)
                return DiagnosticNote(source: articleResult.source, range: linkMarkup.range ?? zeroRange, message: "\(link.destination?.singleQuoted ?? "''") is also documented here.")
            }
            diagnosticEngine.emit(Problem(diagnostic: Diagnostic(source: articleResult.source, severity: .information, range: link.range, identifier: "org.swift.docc.SymbolUnmatched", summary: "No symbol matched \(link.destination?.singleQuoted ?? "''"). This documentation will be ignored.", notes: notes), possibleSolutions: []))
        }
        
        for articleResult in uncuratedArticles.values {
            diagnosticEngine.emit(Problem(diagnostic: Diagnostic(source: articleResult.source, severity: .information, range: nil, identifier: "org.swift.docc.ArticleUncurated", summary: "You haven't curated \(articleResult.topicGraphNode.reference.description.singleQuoted)"), possibleSolutions: []))
        }
    }
    
    /**
     Analysis that runs after all nodes are successfully registered in the context.
     Useful for checks that need the complete node graph.
     */
    func topicGraphGlobalAnalysis() {
        // Run any checks added to the context.
        let problems = knownIdentifiers.flatMap { reference in
            return checks.flatMap { check in
                return check(self, reference)
            }
        }
        diagnosticEngine.emit(problems)
        
        // Run pre-defined global analysis.
        for node in topicGraph.nodes.values {
            switch node.kind {
            case .tutorial:
                Tutorial.analyze(node, completedContext: self, engine: diagnosticEngine)
            case .tutorialArticle:
                TutorialArticle.analyze(node, completedContext: self, engine: diagnosticEngine)
            default: break
            }
        }
        
        // Run global ``TopicGraph`` global analysis.
        analyzeTopicGraph()
    }

    /**
     Unregister a documentation bundle with this context and clear any cached resources associated with it.
     */
    private func unregister(_ bundle: DocumentationBundle) {
        let referencesToRemove = topicGraph.nodes.keys.filter { reference in
            return reference.bundleIdentifier == bundle.identifier
        }
        
        for reference in referencesToRemove {
            topicGraph.edges[reference]?.removeAll(where: { $0.bundleIdentifier == bundle.identifier })
            topicGraph.reverseEdges[reference]?.removeAll(where: { $0.bundleIdentifier == bundle.identifier })
            topicGraph.nodes[reference] = nil
        }
    }

    // MARK: - Getting documentation relationships

    /**
     Look for a secondary resource among the registered bundles.

     The context tracks resources by file name. If the documentation author specified a resource reference using a
     qualified path, instead of a file name, the context will fail to find that resource.

     - Returns: A `Foundation.Data` object with the data for the given ``ResourceReference``.
     - Throws: ``ContextError/notFound(_:)` if a resource with the given was not found.
     */
    public func resource(with identifier: ResourceReference, trait: DataTraitCollection = .init()) throws -> Data {
        guard let bundle = bundle(identifier: identifier.bundleIdentifier),
              let assetManager = assetManagers[identifier.bundleIdentifier],
              let asset = assetManager.allData(named: identifier.path) else {
            throw ContextError.notFound(identifier.url)
        }
        
        let resource = asset.data(bestMatching: trait)
        
        return try dataProvider.contentsOfURL(resource.url, in: bundle)
    }
    
    /// Returns true if a resource with the given identifier exists in the registered bundle.
    public func resourceExists(with identifier: ResourceReference, ofType expectedAssetType: AssetType? = nil) -> Bool {
        guard let assetManager = assetManagers[identifier.bundleIdentifier] else {
            return false
        }
        
        guard let key = assetManager.bestKey(forAssetName: identifier.path) else {
            return false
        }
        
        guard let expectedAssetType = expectedAssetType, let asset = assetManager.storage[key] else {
            return true
        }
        
        return asset.hasVariant(withAssetType: expectedAssetType)
    }
    
    /**
     Returns an externally resolved node for the given reference.

     - Returns: A ``DocumentationNode`` with the given identifier or `nil`.
     - Throws: Re-throws any errors that a registered external resolver might throw for this lookup.
     */
    func externalEntity(with reference: ResolvedTopicReference) throws -> DocumentationNode? {
        return try externalReferenceResolvers[reference.bundleIdentifier].map({ try $0.entity(with: reference) }) ??
            fallbackReferenceResolvers[reference.bundleIdentifier].flatMap({ try $0.entityIfPreviouslyResolved(with: reference) })
    }
    
    /**
     Look for a documentation node among the registered bundles and via any external resolvers.

     - Returns: A ``DocumentationNode`` with the given identifier.
     - Throws: ``ContextError/notFound(_:)`` if a documentation node with the given identifier was not found.
     */
    public func entity(with reference: ResolvedTopicReference) throws -> DocumentationNode {
        let reference = documentationCacheBasedLinkResolver.canonicalReference(for: reference)
        if let cached = documentationCache[reference] {
            return cached
        }
        
        // TODO: We should not mutate the context during rendering and this method is called from `RenderNodeTranslator`
        // We should use a read-only context during render time (rdar://65130130).
        // Try to resolve the reference if an external resolver exists for the reference's bundle identifier
        if let externallyResolved = try externalEntity(with: reference) {
            documentationCache[reference] = externallyResolved
            return externallyResolved
        }
        
        throw ContextError.notFound(reference.url)
    }
    
    /// Returns the set of languages the entity corresponding to the given reference is available in.
    ///
    /// - Precondition: The entity associated with the given reference must be registered in the context.
    public func sourceLanguages(for reference: ResolvedTopicReference) -> Set<SourceLanguage> {
        do {
            // Look up the entity without its fragment. The documentation context does not keep track of page sections
            // as nodes, and page sections are considered to be available in the same languages as the page they're
            // defined in.
            let referenceWithoutFragment = reference.withFragment(nil)
            return try entity(with: referenceWithoutFragment).availableSourceLanguages
        } catch ContextError.notFound {
            preconditionFailure("Reference does not have an associated documentation node.")
        } catch {
            fatalError("Unexpected error when retrieving source languages: \(error)")
        }
    }

    // MARK: - Relationship queries
    
    /// Fetch the child nodes of a documentation node with the given `reference``,  optionally filtering to only children of the given `kind`.
    ///
    /// - Parameters:
    ///   - reference: The reference of the node to fetch children for.
    ///   - kind: An optional documentation node kind to filter the children by.
    /// - Returns: A list of the reference and kind for each matching child node.
    public func children(of reference: ResolvedTopicReference, kind: DocumentationNode.Kind? = nil) -> [(reference: ResolvedTopicReference, kind: DocumentationNode.Kind)] {
        guard let node = topicGraph.nodeWithReference(reference) else {
            return []
        }
        return topicGraph[node].compactMap {
            guard let node = topicGraph.nodeWithReference($0) else {
                return nil
            }
            if kind == nil || node.kind == kind {
                return ($0, node.kind)
            }
            return nil
        }
    }
    
    /// Fetches the parents of the documentation node with the given `reference`.
    ///
    /// - Parameter reference: The reference of the node to fetch parents for.
    /// - Returns: A list of the reference for the given node's parent nodes.
    public func parents(of reference: ResolvedTopicReference) -> [ResolvedTopicReference] {
        return topicGraph.reverseEdges[reference] ?? []
    }
    
    /// Returns the document URL for the given article or tutorial reference.
    ///
    /// - Parameter reference: The identifier for the topic whose file URL to locate.
    /// - Returns: If the reference is a reference to a known Markdown document, this function returns the article's URL, otherwise `nil`.
    @available(*, deprecated, renamed: "documentURL(for:)")
    public func fileURL(for reference: ResolvedTopicReference) -> URL? {
        documentURL(for: reference)
    }
    
    /// Returns the document URL for the given article or tutorial reference.
    ///
    /// - Parameter reference: The identifier for the topic whose file URL to locate.
    /// - Returns: If the reference is a reference to a known Markdown document, this function returns the article's URL, otherwise `nil`.
    public func documentURL(for reference: ResolvedTopicReference) -> URL? {
        if let node = topicGraph.nodes[reference], case .file(let url) = node.source {
            return url
        }
        return nil
    }
    
    /// Returns the URL of the documentation extension of the given reference.
    ///
    /// - Parameter reference: The reference to the symbol this function should return the documentation extension URL for.
    /// - Returns: The document URL of the given symbol reference. If the given reference is not a symbol reference, returns `nil`.
    public func documentationExtensionURL(for reference: ResolvedTopicReference) -> URL? {
        guard (try? entity(with: reference))?.kind.isSymbol == true else {
            return nil
        }
        return documentLocationMap[reference]
    }
    
    /// Attempt to locate the reference for a given file.
    ///
    /// - Parameter url: The file whose reference to locate.
    /// - Returns: The reference for the file if it could be found, otherwise `nil`.
    public func referenceForFileURL(_ url: URL) -> ResolvedTopicReference? {
        return documentLocationMap[url]
    }

    /**
     Attempt to retrieve the title for a given `reference`.
     
     - Parameter reference: The reference for the topic whose title is desired.
     - Returns: The title of the topic if it could be found, otherwise `nil`.
     */
    public func title(for reference: ResolvedTopicReference) -> String? {
        return topicGraph.nodes[reference]?.title
    }
    
    /**
     Traverse the Topic Graph breadth-first, starting at the given reference.
     */
    func traverseBreadthFirst(from reference: ResolvedTopicReference, _ observe: (TopicGraph.Node) -> TopicGraph.Traversal) {
        guard let node = topicGraph.nodeWithReference(reference) else {
            return
        }
        
        topicGraph.traverseBreadthFirst(from: node, observe)
    }
    
    /**
     Attempt to resolve a ``TopicReference``.
     
     > Note: If the reference is already resolved, the original reference is returned.
     
     - Parameters:
       - reference: An unresolved (or resolved) reference.
       - parent: The *resolved* reference that serves as an enclosing search context, especially the parent reference's bundle identifier.
       - fromSymbolLink: If `true` will try to resolve relative links *only* in documentation symbol locations in the hierarchy. If `false` it will try to resolve relative links as tutorials, articles, symbols, etc.
     - Returns: Either the successfully resolved reference for the topic or error information about why the reference couldn't resolve.
     */
    public func resolve(_ reference: TopicReference, in parent: ResolvedTopicReference, fromSymbolLink isCurrentlyResolvingSymbolLink: Bool = false) -> TopicReferenceResolutionResult {
        switch reference {
        case .unresolved(let unresolvedReference):
            if LinkResolutionMigrationConfiguration.shouldReportLinkResolutionResultMismatches {
                let hierarchyBasedResult = hierarchyBasedLinkResolver!.resolve(unresolvedReference, in: parent, fromSymbolLink: isCurrentlyResolvingSymbolLink, context: self)
                let cacheBasedResult = documentationCacheBasedLinkResolver.resolve(unresolvedReference, in: parent, fromSymbolLink: isCurrentlyResolvingSymbolLink, context: self)
                
                let inputInfo = LinkResolutionMismatches.FailedLinkInfo(
                    path: unresolvedReference.topicURL.url.path + (unresolvedReference.topicURL.url.fragment.map { "#" + $0 } ?? ""),
                    parent: parent.url.path,
                    asSymbolLink: isCurrentlyResolvingSymbolLink
                )
                switch (hierarchyBasedResult, cacheBasedResult) {
                case (.success, .failure):
                    linkResolutionMismatches.mismatchedLinksThatCacheBasedLinkResolverFailedToResolve.sync { $0.insert(inputInfo) }
                case (.failure, .success):
                    linkResolutionMismatches.mismatchedLinksThatHierarchyBasedLinkResolverFailedToResolve.sync { $0.insert(inputInfo) }
                default:
                    break // No difference to report
                }
                
                return LinkResolutionMigrationConfiguration.shouldUseHierarchyBasedLinkResolver
                    ? hierarchyBasedResult
                    : cacheBasedResult
            }
            
            if LinkResolutionMigrationConfiguration.shouldUseHierarchyBasedLinkResolver {
                return hierarchyBasedLinkResolver!.resolve(unresolvedReference, in: parent, fromSymbolLink: isCurrentlyResolvingSymbolLink, context: self)
            } else {
                return documentationCacheBasedLinkResolver.resolve(unresolvedReference, in: parent, fromSymbolLink: isCurrentlyResolvingSymbolLink, context: self)
            }
        case .resolved(let resolved):
            // This reference is already resolved (either as a success or a failure), so don't change anything.
            return resolved
        }
    }
    
    /// Update the asset with a new value given the assets name and the topic it's referenced in.
    ///
    /// - Parameters:
    ///   - name: The name of the asset to update.
    ///   - asset: The new asset for this name.
    ///   - parent: The topic where the asset is referenced.
    public func updateAsset(named name: String, asset: DataAsset, in parent: ResolvedTopicReference) {
        let bundleIdentifier = parent.bundleIdentifier
        assetManagers[bundleIdentifier]?.update(name: name, asset: asset)
    }
    
    /// Attempt to resolve an asset given its name and the topic it's referenced in.
    ///
    /// - Parameters:
    ///   - name: The name of the asset.
    ///   - parent: The topic where the asset is referenced.
    /// - Returns: The data that's associated with a image asset if it was found, otherwise `nil`.
    public func resolveAsset(named name: String, in parent: ResolvedTopicReference, withType type: AssetType? = nil) -> DataAsset? {
        let bundleIdentifier = parent.bundleIdentifier
        return resolveAsset(named: name, bundleIdentifier: bundleIdentifier, withType: type)
    }
    
    func resolveAsset(named name: String, bundleIdentifier: String, withType expectedType: AssetType?) -> DataAsset? {
        if let localAsset = assetManagers[bundleIdentifier]?.allData(named: name) {
            if let expectedType = expectedType {
                guard localAsset.hasVariant(withAssetType: expectedType) else {
                    return nil
                }
            }
            
            return localAsset
        }
        
        if let fallbackAssetResolver = fallbackAssetResolvers[bundleIdentifier],
           let externallyResolvedAsset = fallbackAssetResolver.resolve(assetNamed: name, bundleIdentifier: bundleIdentifier) {
            assetManagers[bundleIdentifier, default: DataAssetManager()]
                .register(dataAsset: externallyResolvedAsset, forName: name)
            return externallyResolvedAsset
        }
        
        // If no fallbackAssetResolver is set, try to treat it as external media link
        if let externalMediaLink = URL(string: name),
           externalMediaLink.isAbsoluteWebURL {
            var asset = DataAsset()
            asset.context = .display
            asset.register(externalMediaLink, with: DataTraitCollection(userInterfaceStyle: .light, displayScale: .standard))
            return asset
        }
        return nil
    }
    
    /// Finds the identifier for a given asset name.
    /// 
    /// `name` is one of the following formats:
    /// - "image" - asset name without extension
    /// - "image.png" - asset name including extension
    ///
    /// - Parameters:
    ///   - name: The name of the asset.
    ///   - parent: The topic where the asset is referenced.
    ///
    /// - Returns: The best matching storage key if it was found, otherwise `nil`.
    public func identifier(forAssetName name: String, in parent: ResolvedTopicReference) -> String? {
        let bundleIdentifier = parent.bundleIdentifier
        return assetManagers[bundleIdentifier]?.bestKey(forAssetName: name)
    }

    /// Attempt to resolve an unresolved code listing.
    ///
    /// - Parameters:
    ///   - unresolvedCodeListingReference: The code listing reference to resolve.
    ///   - parent: The topic the code listing reference appears in.
    public func resolveCodeListing(_ unresolvedCodeListingReference: UnresolvedCodeListingReference, in parent: ResolvedTopicReference) -> AttributedCodeListing? {
        return dataProvider.bundles[parent.bundleIdentifier]?.attributedCodeListings[unresolvedCodeListingReference.identifier]
    }
    
    /// The references of all nodes in the topic graph.
    public var knownIdentifiers: [ResolvedTopicReference] {
        return Array(topicGraph.nodes.keys)
    }
    
    /// The references of all the pages in the topic graph.
    public var knownPages: [ResolvedTopicReference] {
        return topicGraph.nodes.values
            .filter { !$0.isVirtual && $0.kind.isPage &&
                !externallyResolvedSymbols.contains($0.reference) }
            .map { $0.reference }
    }
    
    /// Options to consider when producing node breadcrumbs.
    struct PathOptions: OptionSet {
        let rawValue: Int
        
        /// The node is a technology page; sort the path to a technology as canonical.
        static let preferTechnologyRoot = PathOptions(rawValue: 1 << 0)
    }
    
    /// Finds all paths (breadcrumbs) to the given node reference.
    ///
    /// Each path is an array of references to the symbols from the module symbol to the current one.
    /// The first path in the array is always the canonical path to the symbol.
    ///
    /// - Parameters:
    ///   - reference: The reference to build that paths to.
    ///   - currentPathToNode: Used for recursion - an accumulated path to "continue" working on.
    /// - Returns: A list of paths to the current reference in the topic graph.
    func pathsTo(_ reference: ResolvedTopicReference, currentPathToNode: [ResolvedTopicReference] = [], options: PathOptions = []) -> [[ResolvedTopicReference]] {
        let nodeParents = parents(of: reference)
        guard !nodeParents.isEmpty else {
            // The path ends with this node
            return [currentPathToNode]
        }
        var results = [[ResolvedTopicReference]]()
        for parentReference in nodeParents {
            let parentPaths = pathsTo(parentReference, currentPathToNode: [parentReference] + currentPathToNode)
            results.append(contentsOf: parentPaths)
        }
        
        // We are sorting the breadcrumbs by the path distance to the documentation root
        // so that the first element is the shortest path that we are using as canonical.
        results.sort { (lhs, rhs) -> Bool in
            // Order a path rooted in a technology as the canonical one.
            if options.contains(.preferTechnologyRoot), let first = lhs.first {
                return try! entity(with: first).semantic is Technology
            }
            
            // If the breadcrumbs have equal amount of components
            // sort alphabetically to produce stable paths order.
            guard lhs.count != rhs.count else {
                return lhs.map({ $0.path }).joined(separator: ",") < rhs.map({ $0.path }).joined(separator: ",")
            }
            // Order by the length of the breadcrumb.
            return lhs.count < rhs.count
        }
        
        return results
    }
    
    func dumpGraph() -> String {
        return topicGraph.nodes.values
            .filter { parents(of: $0.reference).isEmpty }
            .sorted(by: \.reference.absoluteString)
            .map { node -> String in
                self.topicGraph.dump(startingAt: node, keyPath: \.reference.absoluteString)
            }
            .joined()
    }
    
    private static func defaultLanguage(in sourceLanguages: Set<SourceLanguage>?) -> SourceLanguage {
        sourceLanguages.map { sourceLanguages in
            if sourceLanguages.contains(.swift) {
                return .swift
            } else {
                return sourceLanguages.first ?? .swift
            }
        } ?? SourceLanguage.swift
    }
}

// MARK: - DocumentationCurator
extension DocumentationContext {

    /// The nodes that are allowed to be roots in the topic graph.
    static var allowedRootNodeKinds: [DocumentationNode.Kind] = [.technology, .module]
    
    func analyzeTopicGraph() {
        // Find all nodes that are loose in the graph and have no parent but aren't supposed to
        let unexpectedRoots = topicGraph.nodes.values.filter { node in
            return !DocumentationContext.allowedRootNodeKinds.contains(node.kind)
                && parents(of: node.reference).isEmpty
        }
        let problems = unexpectedRoots.compactMap { node -> Problem? in
            let source: URL
            switch node.source {
            case .file(url: let url): source = url
            case .range(_, let url): source = url
            case .external: return nil
            }
            return Problem(diagnostic: Diagnostic(source: source, severity: .information, range: nil, identifier: "org.swift.docc.SymbolNotCurated", summary: "You haven't curated \(node.reference.absoluteString.singleQuoted)"), possibleSolutions: [Solution(summary: "Add a link to \(node.reference.absoluteString.singleQuoted) from a Topics group of another documentation node.", replacements: [])])
        }
        diagnosticEngine.emit(problems)
    }
}

extension GraphCollector.GraphKind {
    var fileURL: URL {
        switch self {
        case .primary(let url): return url
        case .extension(let url): return url
        }
    }
}

extension SymbolGraphLoader {
    func mainModuleURL(forModule moduleName: String) -> URL? {
        guard let graphURLs = self.graphLocations[moduleName] else { return nil }

        if let firstPrimary: URL = graphURLs.compactMap({
            if case let .primary(url) = $0 {
                return url
            } else {
                return nil
            }
        }).first {
            return firstPrimary
        } else {
            return graphURLs.first.map({ $0.fileURL })
        }
    }

    func hasPrimaryURL(moduleName: String) -> Bool {
        guard let graphURLs = self.graphLocations[moduleName] else { return false }

        return graphURLs.contains(where: {
            if case .primary(_) = $0 {
                return true
            } else {
                return false
            }
        })
    }
}

extension DataAsset {
    fileprivate func hasVariant(withAssetType assetType: DocumentationContext.AssetType) -> Bool {
        return variants.values.map(\.pathExtension).contains { pathExtension in
            return DocumentationContext.isFileExtension(pathExtension, supported: assetType)
        }
    }
}
