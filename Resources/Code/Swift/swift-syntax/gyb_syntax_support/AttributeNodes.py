from .Child import Child
from .Node import Node  # noqa: I201

ATTRIBUTE_NODES = [
    # token-list -> token? token-list?
    Node('TokenList', name_for_diagnostics='token list', kind='SyntaxCollection',
         element='Token'),

    # token-list -> token token-list?
    Node('NonEmptyTokenList', name_for_diagnostics='token list',
         kind='SyntaxCollection', element='Token', omit_when_empty=True),

    Node('CustomAttribute', name_for_diagnostics='attribute', kind='Syntax',
         description='''
         A custom `@` attribute.
         ''',
         children=[
             Child('AtSignToken', kind='AtSignToken',
                   description='The `@` sign.'),
             Child('AttributeName', kind='Type', name_for_diagnostics='name',
                   classification='Attribute',
                   description='The name of the attribute.'),
             Child('LeftParen', kind='LeftParenToken',
                   is_optional=True),
             Child('ArgumentList', kind='TupleExprElementList',
                   collection_element_name='Argument', is_optional=True),
             Child('RightParen', kind='RightParenToken',
                   is_optional=True),
         ]),

    # attribute -> '@' identifier '('?
    #              ( identifier
    #                | string-literal
    #                | integer-literal
    #                | availability-spec-list
    #                | specialize-attr-spec-list
    #                | implements-attr-arguments
    #                | named-attribute-string-argument
    #                | back-deploy-attr-spec-list
    #              )? ')'?
    Node('Attribute', name_for_diagnostics='attribute', kind='Syntax',
         description='''
         An `@` attribute.
         ''',
         children=[
             Child('AtSignToken', kind='AtSignToken',
                   description='The `@` sign.'),
             Child('AttributeName', kind='Token', name_for_diagnostics='name',
                   classification='Attribute',
                   description='The name of the attribute.'),
             Child('LeftParen', kind='LeftParenToken', is_optional=True,
                   description='''
                   If the attribute takes arguments, the opening parenthesis.
                   '''),
             Child('Argument', kind='Syntax', is_optional=True,
                   node_choices=[
                       Child('Identifier', kind='IdentifierToken'),
                       Child('String', kind='StringLiteralToken'),
                       Child('Integer', kind='IntegerLiteralToken'),
                       Child('Availability', kind='AvailabilitySpecList'),
                       Child('SpecializeArguments',
                             kind='SpecializeAttributeSpecList'),
                       Child('ObjCName', kind='ObjCSelector'),
                       Child('ImplementsArguments',
                             kind='ImplementsAttributeArguments'),
                       Child('DifferentiableArguments',
                             kind='DifferentiableAttributeArguments'),
                       Child('DerivativeRegistrationArguments',
                             kind='DerivativeRegistrationAttributeArguments'),
                       Child('NamedAttributeString',
                             kind='NamedAttributeStringArgument'),
                       Child('BackDeployArguments',
                             kind='BackDeployAttributeSpecList'),
                       Child('ConventionArguments',
                             kind='ConventionAttributeArguments'),
                       Child('ConventionWitnessMethodArguments',
                             kind='ConventionWitnessMethodAttributeArguments'),
                       # TokenList for custom effects which are parsed by
                       # `FunctionEffects.parse()` in swift.
                       Child('TokenList', kind='TokenList',
                             collection_element_name='Token'),
                   ], description='''
                   The arguments of the attribute. In case the attribute
                   takes multiple arguments, they are gather in the
                   appropriate takes first.
                   '''),
             Child('RightParen', kind='RightParenToken', is_optional=True,
                   description='''
                   If the attribute takes arguments, the closing parenthesis.
                   '''),
             # TokenList to gather remaining tokens of invalid attributes
             # FIXME: Remove this recovery option entirely
             Child('TokenList', kind='TokenList',
                   collection_element_name='Token', is_optional=True),
         ]),

    # attribute-list -> attribute attribute-list?
    Node('AttributeList', name_for_diagnostics='attributes', kind='SyntaxCollection',
         omit_when_empty=True,
         element='Syntax', element_name='Attribute',
         element_choices=[
             'Attribute',
             'CustomAttribute',
         ]),

    # The argument of '@_specialize(...)'
    # specialize-attr-spec-list -> labeled-specialize-entry
    #                                  specialize-spec-attr-list?
    #                            | generic-where-clause
    #                                  specialize-spec-attr-list?
    Node('SpecializeAttributeSpecList',
         name_for_diagnostics="argument to '@_specialize", kind='SyntaxCollection',
         description='''
         A collection of arguments for the `@_specialize` attribute
         ''',
         element='Syntax', element_name='SpecializeAttribute',
         element_choices=[
             'LabeledSpecializeEntry',
             'AvailabilityEntry',
             'TargetFunctionEntry',
             'GenericWhereClause',
         ]),

    Node('AvailabilityEntry', name_for_diagnostics='availability entry', kind='Syntax',
         description='''
         The availability argument for the _specialize attribute
         ''',
         children=[
             Child('Label', kind='IdentifierToken', name_for_diagnostics='label',
                   description='The label of the argument'),
             Child('Colon', kind='ColonToken',
                   description='The colon separating the label and the value'),
             Child('AvailabilityList', kind='AvailabilitySpecList',
                   collection_element_name='Availability'),
             Child('Semicolon', kind='SemicolonToken'),
         ]),

    # Representation of e.g. 'exported: true,'
    # labeled-specialize-entry -> identifier ':' token ','?
    Node('LabeledSpecializeEntry', kind='Syntax',
         name_for_diagnostics='attribute argument',
         description='''
         A labeled argument for the `@_specialize` attribute like
         `exported: true`
         ''',
         traits=['WithTrailingComma'],
         children=[
             Child('Label', kind='IdentifierToken', name_for_diagnostics='label',
                   description='The label of the argument'),
             Child('Colon', kind='ColonToken',
                   description='The colon separating the label and the value'),
             Child('Value', kind='Token', name_for_diagnostics='value',
                   description='The value for this argument'),
             Child('TrailingComma', kind='CommaToken',
                   is_optional=True, description='''
                   A trailing comma if this argument is followed by another one
                   '''),
         ]),
    # Representation of e.g. 'exported: true,'
    # labeled-specialize-entry -> identifier ':' token ','?
    Node('TargetFunctionEntry', kind='Syntax',
         name_for_diagnostics='attribute argument',
         description='''
         A labeled argument for the `@_specialize` attribute with a function
         decl value like
         `target: myFunc(_:)`
         ''',
         traits=['WithTrailingComma'],
         children=[
             Child('Label', kind='IdentifierToken', name_for_diagnostics='label',
                   description='The label of the argument'),
             Child('Colon', kind='ColonToken',
                   description='The colon separating the label and the value'),
             Child('Declname', kind='DeclName', name_for_diagnostics='declaration name',
                   description='The value for this argument'),
             Child('TrailingComma', kind='CommaToken',
                   is_optional=True, description='''
                   A trailing comma if this argument is followed by another one
                   '''),
         ]),

    # The argument of '@_dynamic_replacement(for:)' or '@_private(sourceFile:)'
    # named-attribute-string-arg -> 'name': string-literal
    Node('NamedAttributeStringArgument', kind='Syntax',
         name_for_diagnostics='attribute argument',
         description='''
         The argument for the `@_dynamic_replacement` or `@_private`
         attribute of the form `for: "function()"` or `sourceFile:
         "Src.swift"`
         ''',
         children=[
             Child('NameTok', kind='Token', name_for_diagnostics='label',
                   description='The label of the argument'),
             Child('Colon', kind='ColonToken',
                   description='The colon separating the label and the value'),
             Child('StringOrDeclname', kind='Syntax', name_for_diagnostics='value', node_choices=[
                 Child('String', kind='StringLiteralToken'),
                 Child('Declname', kind='DeclName'),
             ]),
         ]),
    Node('DeclName', name_for_diagnostics='declaration name', kind='Syntax', children=[
         Child('DeclBaseName', kind='Syntax', name_for_diagnostics='base name', description='''
               The base name of the protocol\'s requirement.
               ''',
               node_choices=[
                   Child('Identifier', kind='IdentifierToken'),
                   Child('Operator', kind='PrefixOperatorToken'),
               ]),
         Child('DeclNameArguments', kind='DeclNameArguments', name_for_diagnostics='arguments',
               is_optional=True, description='''
               The argument labels of the protocol\'s requirement if it
               is a function requirement.
               '''),
         ]),
    # The argument of '@_implements(...)'
    # implements-attr-arguments -> simple-type-identifier ','
    #                              (identifier | operator) decl-name-arguments
    Node('ImplementsAttributeArguments', name_for_diagnostics='@_implements arguemnts',
         kind='Syntax',
         description='''
         The arguments for the `@_implements` attribute of the form
         `Type, methodName(arg1Label:arg2Label:)`
         ''',
         children=[
             Child('Type', kind='Type', name_for_diagnostics='type', description='''
                   The type for which the method with this attribute
                   implements a requirement.
                   '''),
             Child('Comma', kind='CommaToken',
                   description='''
                   The comma separating the type and method name
                   '''),
             Child('DeclBaseName', kind='Token', name_for_diagnostics='declaration base name', description='''
                   The base name of the protocol\'s requirement.
                   '''),
             Child('DeclNameArguments', name_for_diagnostics='declaration name arguments', kind='DeclNameArguments',
                   is_optional=True, description='''
                   The argument labels of the protocol\'s requirement if it
                   is a function requirement.
                   '''),
         ]),

    # objc-selector-piece -> identifier? ':'?
    Node('ObjCSelectorPiece', name_for_diagnostics='Objective-C selector piece',
         kind='Syntax',
         description='''
         A piece of an Objective-C selector. Either consisting of just an
         identifier for a nullary selector, an identifier and a colon for a
         labeled argument or just a colon for an unlabeled argument
         ''',
         children=[
             Child('Name', kind='IdentifierToken', name_for_diagnostics='name', is_optional=True),
             Child('Colon', kind='ColonToken', is_optional=True),
         ]),

    # objc-selector -> objc-selector-piece objc-selector?
    Node('ObjCSelector', name_for_diagnostics='Objective-C selector',
         kind='SyntaxCollection', element='ObjCSelectorPiece'),

    # The argument of '@differentiable(...)'.
    # differentiable-attr-arguments ->
    #     differentiability-kind? '.'? differentiability-params-clause? ','?
    #     generic-where-clause?
    Node('DifferentiableAttributeArguments',
         name_for_diagnostics="'@differentiable' arguments", kind='Syntax',
         description='''
         The arguments for the `@differentiable` attribute: an optional
         differentiability kind, an optional differentiability parameter clause,
         and an optional 'where' clause.
         ''',
         children=[
             Child('DiffKind', kind='IdentifierToken',
                   text_choices=['forward', 'reverse', 'linear'],
                   is_optional=True),
             Child('DiffKindComma', kind='CommaToken', description='''
                   The comma following the differentiability kind, if it exists.
                   ''', is_optional=True),
             Child('DiffParams', kind='DifferentiabilityParamsClause',
                   is_optional=True),
             Child('DiffParamsComma', kind='CommaToken', description='''
                   The comma following the differentiability parameters clause,
                   if it exists.
                   ''', is_optional=True),
             Child('WhereClause', kind='GenericWhereClause', is_optional=True),
         ]),

    # differentiability-params-clause ->
    #     'wrt' ':' (differentiability-param | differentiability-params)
    Node('DifferentiabilityParamsClause',
         name_for_diagnostics="'@differentiable' argument", kind='Syntax',
         description='A clause containing differentiability parameters.',
         children=[
             Child('WrtLabel', kind='IdentifierToken',
                   text_choices=['wrt'], description='The "wrt" label.'),
             Child('Colon', kind='ColonToken', description='''
                   The colon separating "wrt" and the parameter list.
                   '''),
             Child('Parameters', kind='Syntax', name_for_diagnostics='parameters',
                   node_choices=[
                       Child('Parameter', kind='DifferentiabilityParam'),
                       Child('ParameterList', kind='DifferentiabilityParams'),
                   ]),
         ]),

    # differentiability-params -> '(' differentiability-param-list ')'
    Node('DifferentiabilityParams', name_for_diagnostics='differentiability parameters',
         kind='Syntax',
         description='The differentiability parameters.',
         children=[
             Child('LeftParen', kind='LeftParenToken'),
             Child('DiffParams', kind='DifferentiabilityParamList',
                   collection_element_name='DifferentiabilityParam',
                   description='The parameters for differentiation.'),
             Child('RightParen', kind='RightParenToken'),
         ]),

    # differentiability-param-list ->
    #     differentiability-param differentiability-param-list?
    Node('DifferentiabilityParamList',
         name_for_diagnostics='differentiability parameters', kind='SyntaxCollection',
         element='DifferentiabilityParam'),

    # differentiability-param -> ('self' | identifier | integer-literal) ','?
    Node('DifferentiabilityParam', name_for_diagnostics='differentiability parameter',
         kind='Syntax',
         description='''
         A differentiability parameter: either the "self" identifier, a function
         parameter name, or a function parameter index.
         ''',
         traits=['WithTrailingComma'],
         children=[
             Child('Parameter', kind='Syntax',
                   node_choices=[
                       Child('Self', kind='SelfToken'),
                       Child('Name', kind='IdentifierToken'),
                       Child('Index', kind='IntegerLiteralToken'),
                   ]),
             Child('TrailingComma', kind='CommaToken', is_optional=True),
         ]),

    # The argument of the derivative registration attribute
    # '@derivative(of: ...)' and the transpose registration attribute
    # '@transpose(of: ...)'.
    #
    # derivative-registration-attr-arguments ->
    #     'of' ':' func-decl-name ','? differentiability-params-clause?
    Node('DerivativeRegistrationAttributeArguments',
         name_for_diagnostics='attribute arguments',
         kind='Syntax',
         description='''
         The arguments for the '@derivative(of:)' and '@transpose(of:)'
         attributes: the 'of:' label, the original declaration name, and an
         optional differentiability parameter list.
         ''',
         children=[
             Child('OfLabel', kind='IdentifierToken', text_choices=['of'],
                   description='The "of" label.'),
             Child('Colon', kind='ColonToken', description='''
                   The colon separating the "of" label and the original
                   declaration name.
                   '''),
             Child('OriginalDeclName', kind='QualifiedDeclName',
                   description='The referenced original declaration name.'),
             Child('Period', kind='PeriodToken',
                   description='''
                   The period separating the original declaration name and the
                   accessor name.
                   ''', is_optional=True),
             Child('AccessorKind', kind='IdentifierToken',
                   description='The accessor name.',
                   text_choices=['get', 'set'],
                   is_optional=True),
             Child('Comma', kind='CommaToken', is_optional=True),
             Child('DiffParams', kind='DifferentiabilityParamsClause',
                   is_optional=True),
         ]),

    # An optionally qualified declaration name.
    # Currently used only for `@derivative` and `@transpose` attribute.
    # TODO(TF-1066): Use module qualified name syntax/parsing instead of custom
    # qualified name syntax/parsing.
    #
    # qualified-decl-name ->
    #     base-type? '.'? (identifier | operator) decl-name-arguments?
    # base-type ->
    #     member-type-identifier | base-type-identifier
    Node('QualifiedDeclName', kind='Syntax',
         name_for_diagnostics='declaration name',
         description='''
         An optionally qualified function declaration name (e.g. `+(_:_:)`,
         `A.B.C.foo(_:_:)`).
         ''',
         children=[
             Child('BaseType', kind='Type', name_for_diagnostics='base type', description='''
                   The base type of the qualified name, optionally specified.
                   ''', is_optional=True),
             Child('Dot', kind='Token',
                   token_choices=[
                       'PeriodToken', 'PrefixPeriodToken'
                   ], is_optional=True),
             Child('Name', kind='Token', name_for_diagnostics='base name', description='''
                   The base name of the referenced function.
                   ''',
                   token_choices=[
                       'IdentifierToken',
                       'UnspacedBinaryOperatorToken',
                       'SpacedBinaryOperatorToken',
                       'PrefixOperatorToken',
                       'PostfixOperatorToken',
                   ]),
             Child('Arguments', name_for_diagnostics='arguments', kind='DeclNameArguments',
                   is_optional=True, description='''
                   The argument labels of the referenced function, optionally
                   specified.
                   '''),
         ]),

    # func-decl-name -> (identifier | operator) decl-name-arguments?
    # NOTE: This is duplicated with `DeclName` above. Change `DeclName`
    # description and use it if possible.
    Node('FunctionDeclName', kind='Syntax',
         name_for_diagnostics='function declaration name',
         description='A function declaration name (e.g. `foo(_:_:)`).',
         children=[
             Child('Name', kind='Syntax', name_for_diagnostics='base name', description='''
                   The base name of the referenced function.
                   ''',
                   node_choices=[
                       Child('Identifier', kind='IdentifierToken'),
                       Child('PrefixOperator', kind='PrefixOperatorToken'),
                       Child('SpacedBinaryOperator',
                             kind='SpacedBinaryOperatorToken'),
                   ]),
             Child('Arguments', name_for_diagnostics='arguments', kind='DeclNameArguments',
                   is_optional=True, description='''
                   The argument labels of the referenced function, optionally
                   specified.
                   '''),
         ]),

    # The arguments of '@_backDeploy(...)'
    # back-deploy-attr-spec-list -> 'before' ':' back-deploy-version-list
    Node('BackDeployAttributeSpecList', kind='Syntax',
         name_for_diagnostics="'@_backDeploy' arguments",
         description='''
         A collection of arguments for the `@_backDeploy` attribute
         ''',
         children=[
             Child('BeforeLabel', kind='IdentifierToken',
                   text_choices=['before'], description='The "before" label.'),
             Child('Colon', kind='ColonToken', description='''
                   The colon separating "before" and the parameter list.
                   '''),
             Child('VersionList', kind='BackDeployVersionList',
                   collection_element_name='Availability', description='''
                   The list of OS versions in which the declaration became ABI
                   stable.
                   '''),
         ]),

    # back-deploy-version-list ->
    #   back-deploy-version-entry back-deploy-version-list?
    Node('BackDeployVersionList', name_for_diagnostics='version list',
         kind='SyntaxCollection', element='BackDeployVersionArgument'),

    # back-deploy-version-entry -> availability-version-restriction ','?
    Node('BackDeployVersionArgument', name_for_diagnostics='version', kind='Syntax',
         description='''
         A single platform/version pair in a `@_backDeploy` attribute,
         e.g. `iOS 10.1`.
         ''',
         children=[
             Child('AvailabilityVersionRestriction',
                   kind='AvailabilityVersionRestriction',
                   classification='Keyword'),
             Child('TrailingComma', kind='CommaToken', is_optional=True,
                   description='''
                   A trailing comma if the argument is followed by another
                   argument
                   '''),
         ]),
    # opaque-return-type-of-arguments -> string-literal ',' integer-literal
    Node('OpaqueReturnTypeOfAttributeArguments',
         name_for_diagnostics='opaque return type arguments',
         kind='Syntax',
         description='''
         The arguments for the '@_opaqueReturnTypeOf()'.
         ''',
         children=[
             Child('MangledName', kind='StringLiteralToken',
                   description='The mangled name of a declaration.'),
             Child('Comma', kind='CommaToken'),
             Child('Ordinal', kind='IntegerLiteralToken',
                   description="The ordinal corresponding to the 'some' keyword that introduced this opaque type."),
        ]),

    # convention-attribute-arguments -> token ',' 'cType'? ':' string-literal
    Node('ConventionAttributeArguments',
         name_for_diagnostics='@convention(...) arguments',
         kind='Syntax',
         description='''
         The arguments for the '@convention(...)'.
         ''',
         children=[
             Child('ConventionLabel', kind='IdentifierToken',
                   text_choices=['block', 'c', 'objc_method', 'thin', 'thick'],
                   description='The convention label.'),
             Child('Comma', kind='CommaToken', is_optional=True),
             Child('CTypeLabel', kind='IdentifierToken',
                   text_choices=['cType'], is_optional=True),
             Child('Colon', kind='ColonToken', is_optional=True),
             Child('CTypeString', kind='StringLiteralToken', is_optional=True),
        ]),

    # convention-attribute-arguments -> 'witness_method' ':' identifier
    Node('ConventionWitnessMethodAttributeArguments',
         name_for_diagnostics='@convention(...) arguments for witness methods',
         kind='Syntax',
         description='''
         The arguments for the '@convention(witness_method: ...)'.
         ''',
         children=[
             Child('WitnessMethodLabel', kind='IdentifierToken'),
             Child('Colon', kind='ColonToken'),
             Child('ProtocolName', kind='IdentifierToken'),
        ]),
]
