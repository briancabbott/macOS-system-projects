# flake8: noqa I201
from .Child import Child
from .Node import Node


DECL_NODES = [
    # type-assignment -> '=' type
    Node('TypeInitializerClause', name_for_diagnostics=None, kind='Syntax',
         children=[
             Child('Equal', kind='EqualToken'),
             Child('Value', kind='Type', name_for_diagnostics='value'),
         ]),

    # typealias-declaration -> attributes? access-level-modifier? 'typealias'
    #                            typealias-name generic-parameter-clause?
    #                            type-assignment
    # typealias-name -> identifier
    Node('TypealiasDecl', name_for_diagnostics='typealias declaration', kind='Decl',
         traits=['IdentifiedDecl'],
         children=[
             Child('Attributes', kind='AttributeList', name_for_diagnostics='attributes',
                   collection_element_name='Attribute', is_optional=True),
             Child('Modifiers', kind='ModifierList', name_for_diagnostics='modifiers',
                   collection_element_name='Modifier', is_optional=True),
             Child('TypealiasKeyword', kind='TypealiasToken'),
             Child('Identifier', kind='IdentifierToken',  name_for_diagnostics='name'),
             Child('GenericParameterClause', kind='GenericParameterClause', name_for_diagnostics='generic parameter clause',
                   is_optional=True),
             Child('Initializer', kind='TypeInitializerClause'),
             Child('GenericWhereClause', kind='GenericWhereClause', name_for_diagnostics='generic where clause',
                   is_optional=True),
         ]),

    # associatedtype-declaration -> attributes? access-level-modifier?
    #                                 'associatedtype' associatedtype-name
    #                                 inheritance-clause? type-assignment?
    #                                 generic-where-clause?
    # associatedtype-name -> identifier
    Node('AssociatedtypeDecl', name_for_diagnostics='associatedtype declaration',
         kind='Decl', traits=['IdentifiedDecl'],
         children=[
             Child('Attributes', kind='AttributeList', name_for_diagnostics='attributes',
                   collection_element_name='Attribute', is_optional=True),
             Child('Modifiers', kind='ModifierList', name_for_diagnostics='modifiers',
                   collection_element_name='Modifier', is_optional=True),
             Child('AssociatedtypeKeyword', kind='AssociatedtypeToken'),
             Child('Identifier', kind='IdentifierToken',  name_for_diagnostics='name'),
             Child('InheritanceClause', kind='TypeInheritanceClause', name_for_diagnostics='inheritance clause',
                   is_optional=True),
             Child('Initializer', kind='TypeInitializerClause',
                   is_optional=True),
             Child('GenericWhereClause', kind='GenericWhereClause', name_for_diagnostics='generic where clause',
                   is_optional=True),
         ]),

    Node('FunctionParameterList', name_for_diagnostics='function parameter list',
         kind='SyntaxCollection', element='FunctionParameter'),

    Node('ParameterClause', name_for_diagnostics='parameter clause', kind='Syntax',
         traits=['Parenthesized'],
         children=[
             Child('LeftParen', kind='LeftParenToken'),
             Child('ParameterList', kind='FunctionParameterList', name_for_diagnostics='parameters',
                   collection_element_name='Parameter'),
             Child('RightParen', kind='RightParenToken'),
         ]),

    # -> Type
    Node('ReturnClause', name_for_diagnostics=None, kind='Syntax',
         children=[
             Child('Arrow', kind='ArrowToken'),
             Child('ReturnType', kind='Type', name_for_diagnostics='return type'),
         ]),

    # function-signature ->
    #   '(' parameter-list? ')' async? (throws | rethrows)? '->'? type?
    Node('FunctionSignature', name_for_diagnostics='function signature', kind='Syntax',
         children=[
             Child('Input', kind='ParameterClause'),
             Child('AsyncOrReasyncKeyword', kind='ContextualKeywordToken',
                   text_choices=['async', 'reasync'], is_optional=True),
             Child('ThrowsOrRethrowsKeyword', kind='Token',
                   is_optional=True,
                   token_choices=[
                       'ThrowsToken',
                       'RethrowsToken',
                   ]),
             Child('Output', kind='ReturnClause', is_optional=True),
         ]),

    # if-config-clause ->
    #    ('#if' | '#elseif' | '#else') expr? (stmt-list | switch-case-list)
    Node('IfConfigClause', name_for_diagnostics="conditional compilation clause",
         kind='Syntax',
         children=[
             Child('PoundKeyword', kind='Token',
                   classification='BuildConfigId',
                   token_choices=[
                       'PoundIfToken',
                       'PoundElseifToken',
                       'PoundElseToken',
                   ]),
             Child('Condition', kind='Expr', name_for_diagnostics='condition', classification='BuildConfigId',
                   is_optional=True),
             Child('Elements', kind='Syntax',
                   node_choices=[
                       Child('Statements', kind='CodeBlockItemList'),
                       Child('SwitchCases', kind='SwitchCaseList'),
                       Child('Decls', kind='MemberDeclList'),
                       Child('PostfixExpression', kind='Expr'),
                   ]),
         ]),

    Node('IfConfigClauseList', name_for_diagnostics=None, kind='SyntaxCollection',
         element='IfConfigClause'),

    # if-config-decl -> '#if' expr stmt-list else-if-directive-clause-list
    #   else-clause? '#endif'
    Node('IfConfigDecl', name_for_diagnostics='conditional compilation block',
         kind='Decl',
         children=[
             Child('Clauses', kind='IfConfigClauseList',
                   collection_element_name='Clause'),
             Child('PoundEndif', kind='PoundEndifToken',
                   classification='BuildConfigId'),
         ]),

    Node('PoundErrorDecl', name_for_diagnostics="'#error' directive", kind='Decl',
         traits=['Parenthesized'],
         children=[
             Child('PoundError', kind='PoundErrorToken'),
             Child('LeftParen', kind='LeftParenToken'),
             Child('Message', kind='StringLiteralExpr', name_for_diagnostics='message'),
             Child('RightParen', kind='RightParenToken')
         ]),

    Node('PoundWarningDecl', name_for_diagnostics="'#warning' directive", kind='Decl',
         traits=['Parenthesized'],
         children=[
             Child('PoundWarning', kind='PoundWarningToken'),
             Child('LeftParen', kind='LeftParenToken'),
             Child('Message', kind='StringLiteralExpr', name_for_diagnostics='message'),
             Child('RightParen', kind='RightParenToken')
         ]),

    Node('PoundSourceLocation', name_for_diagnostics="'#sourceLocation' directive",
         kind='Decl', traits=['Parenthesized'],
         children=[
             Child('PoundSourceLocation', kind='PoundSourceLocationToken'),
             Child('LeftParen', kind='LeftParenToken'),
             Child('Args', kind='PoundSourceLocationArgs', name_for_diagnostics='arguments', is_optional=True),
             Child('RightParen', kind='RightParenToken')
         ]),

    Node('PoundSourceLocationArgs', name_for_diagnostics="'#sourceLocation' arguments",
         kind='Syntax',
         children=[
             Child('FileArgLabel', kind='IdentifierToken',
                   text_choices=['file']),
             Child('FileArgColon', kind='ColonToken'),
             Child('FileName', kind='StringLiteralToken', name_for_diagnostics='file name'),
             Child('Comma', kind='CommaToken'),
             Child('LineArgLabel', kind='IdentifierToken',
                   text_choices=['line']),
             Child('LineArgColon', kind='ColonToken'),
             Child('LineNumber', name_for_diagnostics='line number', kind='IntegerLiteralToken'),
         ]),

    Node('DeclModifierDetail', name_for_diagnostics=None, kind='Syntax',
         traits=['Parenthesized'],
         children=[
             Child('LeftParen', kind='LeftParenToken'),
             Child('Detail', kind='IdentifierToken'),
             Child('RightParen', kind='RightParenToken'),
         ]),

    Node('DeclModifier', name_for_diagnostics='modifier', kind='Syntax',
         children=[
             Child('Name', kind='Token', classification='Attribute',
                   text_choices=[
                       'class', 'convenience', 'dynamic', 'final', 'infix',
                       'lazy', 'optional', 'override', 'postfix', 'prefix',
                       'required', 'static', 'unowned', 'weak', 'private',
                       'fileprivate', 'internal', 'public', 'open',
                       'mutating', 'nonmutating', 'indirect', '__consuming',
                       'actor', 'async', 'distributed', 'isolated',
                       'nonisolated', '_const', '_local'
                   ]),
             Child('Detail', kind='DeclModifierDetail', is_optional=True),
         ]),

    Node('InheritedType', name_for_diagnostics='type', kind='Syntax',
         traits=['WithTrailingComma'],
         children=[
            Child('TypeName', kind='Type'),
            Child('TrailingComma', kind='CommaToken', is_optional=True),
         ]),

    Node('InheritedTypeList', name_for_diagnostics=None, kind='SyntaxCollection',
         element='InheritedType'),

    # type-inheritance-clause -> ':' type
    Node('TypeInheritanceClause', name_for_diagnostics='inheritance clause',
         kind='Syntax',
         children=[
             Child('Colon', kind='ColonToken'),
             Child('InheritedTypeCollection', kind='InheritedTypeList',
                   collection_element_name='InheritedType'),
         ]),

    # class-declaration -> attributes? access-level-modifier?
    #                      'class' class-name
    #                      generic-parameter-clause?
    #                      type-inheritance-clause?
    #                      generic-where-clause?
    #                     '{' class-members '}'
    # class-name -> identifier
    Node('ClassDecl', name_for_diagnostics='class', kind='Decl',
         traits=['DeclGroup', 'IdentifiedDecl'],
         children=[
             Child('Attributes', kind='AttributeList', name_for_diagnostics='attributes',
                   collection_element_name='Attribute', is_optional=True),
             Child('Modifiers', kind='ModifierList', name_for_diagnostics='modifiers',
                   collection_element_name='Modifier', is_optional=True),
             Child('ClassKeyword', kind='ClassToken'),
             Child('Identifier', kind='IdentifierToken', name_for_diagnostics='name'),
             Child('GenericParameterClause', kind='GenericParameterClause', name_for_diagnostics='generic parameter clause',
                   is_optional=True),
             Child('InheritanceClause', kind='TypeInheritanceClause', name_for_diagnostics='inheritance clause',
                   is_optional=True),
             Child('GenericWhereClause', kind='GenericWhereClause', name_for_diagnostics='generic where clause',
                   is_optional=True),
             Child('Members', kind='MemberDeclBlock'),
         ]),

    # actor-declaration -> attributes? access-level-modifier?
    #                      'actor' actor-name
    #                      generic-parameter-clause?
    #                      type-inheritance-clause?
    #                      generic-where-clause?
    #                     '{' actor-members '}'
    # actor-name -> identifier
    Node('ActorDecl', name_for_diagnostics='actor', kind='Decl',
         traits=['DeclGroup', 'IdentifiedDecl'],
         children=[
             Child('Attributes', kind='AttributeList', name_for_diagnostics='attributes',
                   collection_element_name='Attribute', is_optional=True),
             Child('Modifiers', kind='ModifierList', name_for_diagnostics='modifiers',
                   collection_element_name='Modifier', is_optional=True),
             Child('ActorKeyword', kind='ContextualKeywordToken',
                   text_choices=['actor']),
             Child('Identifier', kind='IdentifierToken', name_for_diagnostics='name'),
             Child('GenericParameterClause', kind='GenericParameterClause', name_for_diagnostics='generic parameter clause',
                   is_optional=True),
             Child('InheritanceClause', kind='TypeInheritanceClause', name_for_diagnostics='type inheritance clause',
                   is_optional=True),
             Child('GenericWhereClause', kind='GenericWhereClause', name_for_diagnostics='generic where clause',
                   is_optional=True),
             Child('Members', kind='MemberDeclBlock'),
         ]),
    # struct-declaration -> attributes? access-level-modifier?
    #                         'struct' struct-name
    #                         generic-parameter-clause?
    #                           type-inheritance-clause?
    #                         generic-where-clause?
    #                         '{' struct-members '}'
    # struct-name -> identifier
    Node('StructDecl', name_for_diagnostics='struct', kind='Decl',
         traits=['DeclGroup', 'IdentifiedDecl'],
         children=[
             Child('Attributes', kind='AttributeList', name_for_diagnostics='attributes',
                   collection_element_name='Attribute', is_optional=True),
             Child('Modifiers', kind='ModifierList', name_for_diagnostics='modifiers',
                   collection_element_name='Modifier', is_optional=True),
             Child('StructKeyword', kind='StructToken'),
             Child('Identifier', kind='IdentifierToken', name_for_diagnostics='name'),
             Child('GenericParameterClause', kind='GenericParameterClause', name_for_diagnostics='generic parameter clause',
                   is_optional=True),
             Child('InheritanceClause', kind='TypeInheritanceClause', name_for_diagnostics='type inheritance clause',
                   is_optional=True),
             Child('GenericWhereClause', kind='GenericWhereClause', name_for_diagnostics='generic where clause',
                   is_optional=True),
             Child('Members', kind='MemberDeclBlock'),
         ]),

    Node('ProtocolDecl', name_for_diagnostics='protocol', kind='Decl',
         traits=['DeclGroup', 'IdentifiedDecl'],
         children=[
             Child('Attributes', kind='AttributeList', name_for_diagnostics='attributes',
                   collection_element_name='Attribute', is_optional=True),
             Child('Modifiers', kind='ModifierList', name_for_diagnostics='modifiers',
                   collection_element_name='Modifier', is_optional=True),
             Child('ProtocolKeyword', kind='ProtocolToken'),
             Child('Identifier', kind='IdentifierToken', name_for_diagnostics='name'),
             Child('PrimaryAssociatedTypeClause',
                   kind='PrimaryAssociatedTypeClause',
                   name_for_diagnostics='primary associated type clause',
                   is_optional=True),
             Child('InheritanceClause', kind='TypeInheritanceClause', name_for_diagnostics='inheritance clause',
                   is_optional=True),
             Child('GenericWhereClause', kind='GenericWhereClause', name_for_diagnostics='generic where clause',
                   is_optional=True),
             Child('Members', kind='MemberDeclBlock'),
         ]),

    # extension-declaration -> attributes? access-level-modifier?
    #                            'extension' extended-type
    #                              type-inheritance-clause?
    #                            generic-where-clause?
    #                            '{' extension-members '}'
    # extension-name -> identifier
    Node('ExtensionDecl', name_for_diagnostics='extension', kind='Decl',
         traits=['DeclGroup'],
         children=[
             Child('Attributes', kind='AttributeList', name_for_diagnostics='attributes',
                   collection_element_name='Attribute', is_optional=True),
             Child('Modifiers', kind='ModifierList', name_for_diagnostics='modifiers',
                   collection_element_name='Modifier', is_optional=True),
             Child('ExtensionKeyword', kind='ExtensionToken'),
             Child('ExtendedType', kind='Type', name_for_diagnostics='name'),
             Child('InheritanceClause', kind='TypeInheritanceClause', name_for_diagnostics='inheritance clause',
                   is_optional=True),
             Child('GenericWhereClause', kind='GenericWhereClause', name_for_diagnostics='generic where clause',
                   is_optional=True),
             Child('Members', kind='MemberDeclBlock'),
         ]),

    Node('MemberDeclBlock', name_for_diagnostics=None, kind='Syntax',
         traits=['Braced'],
         parser_function='parseMemberDeclList',
         children=[
             Child('LeftBrace', kind='LeftBraceToken'),
             Child('Members', kind='MemberDeclList',
                   collection_element_name='Member', is_indented=True),
             Child('RightBrace', kind='RightBraceToken',
                   requires_leading_newline=True),
         ]),

    # member-decl-list = member-decl member-decl-list?
    Node('MemberDeclList', name_for_diagnostics=None, kind='SyntaxCollection',
         element='MemberDeclListItem', elements_separated_by_newline=True),

    # member-decl = decl ';'?
    Node('MemberDeclListItem', name_for_diagnostics=None, kind='Syntax',
         omit_when_empty=True,
         description='''
         A member declaration of a type consisting of a declaration and an
         optional semicolon;
         ''',
         children=[
             Child('Decl', kind='Decl',
                   description='The declaration of the type member.'),
             Child('Semicolon', kind='SemicolonToken', is_optional=True,
                   description='An optional trailing semicolon.'),
         ]),

    # source-file = code-block-item-list eof
    Node('SourceFile', name_for_diagnostics='source file', kind='Syntax',
         traits=['WithStatements'],
         parser_function='parseSourceFile',
         children=[
             Child('Statements', kind='CodeBlockItemList',
                   collection_element_name='Statement'),
             Child('EOFToken', kind='EOFToken')
         ]),

    # initializer -> '=' expr
    Node('InitializerClause', name_for_diagnostics=None, kind='Syntax',
         children=[
             Child('Equal', kind='EqualToken'),
             Child('Value', kind='Expr'),
         ]),

    # parameter ->
    # external-parameter-name? local-parameter-name ':'
    #   type '...'? '='? expression? ','?
    Node('FunctionParameter', name_for_diagnostics='function parameter', kind='Syntax',
         traits=['WithTrailingComma'],
         children=[
             Child('Attributes', kind='AttributeList', name_for_diagnostics='attributes',
                   collection_element_name='Attribute', is_optional=True),
             Child('Modifiers', kind='ModifierList', name_for_diagnostics='modifiers',
                   collection_element_name='Modifier', is_optional=True),
             Child('FirstName', kind='Token', name_for_diagnostics='name',
                   token_choices=[
                       'IdentifierToken',
                       'WildcardToken',
                   ],
                   is_optional=True),
             # One of these two names needs be optional, we choose the second
             # name to avoid backtracking.
             Child('SecondName', kind='Token', name_for_diagnostics='internal name',
                   token_choices=[
                       'IdentifierToken',
                       'WildcardToken',
                   ],
                   is_optional=True),
             Child('Colon', kind='ColonToken',
                   is_optional=True),
             Child('Type', kind='Type', name_for_diagnostics='type',
                   is_optional=True),
             Child('Ellipsis', kind='EllipsisToken',
                   is_optional=True),
             Child('DefaultArgument', kind='InitializerClause', name_for_diagnostics='default argument',
                   is_optional=True),
             Child('TrailingComma', kind='CommaToken',
                   is_optional=True),
         ]),

    # declaration-modifier -> access-level-modifier
    #                       | mutation-modifier
    #                       | 'class'
    #                       | 'convenience'
    #                       | 'distributed'
    #                       | 'dynamic'
    #                       | 'final'
    #                       | 'infix'
    #                       | 'isolated'
    #                       | 'lazy'
    #                       | 'nonisolated'
    #                       | 'optional'
    #                       | 'override'
    #                       | 'postfix'
    #                       | 'prefix'
    #                       | 'required'
    #                       | 'static'
    #                       | 'unowned'
    #                       | 'unowned(safe)'
    #                       | 'unowned(unsafe)'
    #                       | 'weak'
    # mutation-modifier -> 'mutating' | 'nonmutating'
    Node('ModifierList', name_for_diagnostics=None, kind='SyntaxCollection',
         omit_when_empty=True,
         element='DeclModifier',
         element_name='Modifier'),

    Node('FunctionDecl', name_for_diagnostics='function', kind='Decl',
         traits=['IdentifiedDecl'],
         children=[
             Child('Attributes', kind='AttributeList', name_for_diagnostics='attributes',
                   collection_element_name='Attribute', is_optional=True),
             Child('Modifiers', kind='ModifierList', name_for_diagnostics='modifiers',
                   collection_element_name='Modifier', is_optional=True),
             Child('FuncKeyword', kind='FuncToken'),
             Child('Identifier', kind='Token', name_for_diagnostics='name',
                   token_choices=[
                       'IdentifierToken',
                       'UnspacedBinaryOperatorToken',
                       'SpacedBinaryOperatorToken',
                       'PrefixOperatorToken',
                       'PostfixOperatorToken',
                   ]),
             Child('GenericParameterClause', kind='GenericParameterClause', name_for_diagnostics='generic parameter clause',
                   is_optional=True),
             Child('Signature', kind='FunctionSignature', name_for_diagnostics='function signature'),
             Child('GenericWhereClause', kind='GenericWhereClause', name_for_diagnostics='generic where clause',
                   is_optional=True),
             # the body is not necessary inside a protocol definition
             Child('Body', kind='CodeBlock', is_optional=True),
         ]),

    Node('InitializerDecl', name_for_diagnostics='initializer', kind='Decl',
         children=[
             Child('Attributes', kind='AttributeList', name_for_diagnostics='attributes',
                   collection_element_name='Attribute', is_optional=True),
             Child('Modifiers', kind='ModifierList', name_for_diagnostics='modifiers',
                   collection_element_name='Modifier', is_optional=True),
             Child('InitKeyword', kind='InitToken'),
             Child('OptionalMark', kind='Token',
                   token_choices=[
                       'PostfixQuestionMarkToken',
                       'InfixQuestionMarkToken',
                       'ExclamationMarkToken',
                   ],
                   is_optional=True),
             Child('GenericParameterClause', kind='GenericParameterClause', name_for_diagnostics='generic parameter clause',
                   is_optional=True),
             Child('Signature', kind='FunctionSignature', name_for_diagnostics='function signature',),
             Child('GenericWhereClause', kind='GenericWhereClause', name_for_diagnostics='generic where clause',
                   is_optional=True),
             # the body is not necessary inside a protocol definition
             Child('Body', kind='CodeBlock', is_optional=True),
         ]),

    Node('DeinitializerDecl', name_for_diagnostics='deinitializer', kind='Decl',
         children=[
             Child('Attributes', kind='AttributeList', name_for_diagnostics='attributes',
                   collection_element_name='Attribute', is_optional=True),
             Child('Modifiers', kind='ModifierList', name_for_diagnostics='modifiers',
                   collection_element_name='Modifier', is_optional=True),
             Child('DeinitKeyword', kind='DeinitToken'),
             Child('Body', kind='CodeBlock', is_optional=True),
         ]),

    Node('SubscriptDecl', name_for_diagnostics='subscript', kind='Decl',
         children=[
             Child('Attributes', kind='AttributeList', name_for_diagnostics='attributes',
                   collection_element_name='Attribute', is_optional=True),
             Child('Modifiers', kind='ModifierList', name_for_diagnostics='modifiers',
                   collection_element_name='Modifier', is_optional=True),
             Child('SubscriptKeyword', kind='SubscriptToken'),
             Child('GenericParameterClause', kind='GenericParameterClause', name_for_diagnostics='generic parameter clause',
                   is_optional=True),
             Child('Indices', kind='ParameterClause'),
             Child('Result', kind='ReturnClause'),
             Child('GenericWhereClause', kind='GenericWhereClause', name_for_diagnostics='generic where clause',
                   is_optional=True),
             # the body is not necessary inside a protocol definition
             Child('Accessor', kind='Syntax', is_optional=True,
                   node_choices=[
                      Child('Accessors', kind='AccessorBlock'),
                      Child('Getter', kind='CodeBlock')]),
         ]),

    # access-level-modifier -> 'private' | 'private' '(' 'set' ')'
    #                        | 'fileprivate' | 'fileprivate' '(' 'set' ')'
    #                        | 'internal' | 'internal' '(' 'set' ')'
    #                        | 'public' | 'public' '(' 'set' ')'
    #                        | 'open' | 'open' '(' 'set' ')'
    Node('AccessLevelModifier', name_for_diagnostics='access level modifier',
         kind='Syntax',
         children=[
             Child('Name', kind='IdentifierToken', name_for_diagnostics='name',),
             Child('Modifier', kind='DeclModifierDetail',
                   is_optional=True),
         ]),

    # FIXME: technically misnamed; should be "ImportPathComponent"
    Node('AccessPathComponent', name_for_diagnostics=None, kind='Syntax',
         children=[
            Child('Name', kind='IdentifierToken', name_for_diagnostics='name',),
            Child('TrailingDot', kind='PeriodToken', is_optional=True),
         ]),

    # FIXME: technically misnamed; should be "ImportPath"
    Node('AccessPath', name_for_diagnostics=None, kind='SyntaxCollection',
         element='AccessPathComponent'),

    Node('ImportDecl', name_for_diagnostics='import', kind='Decl',
         children=[
             Child('Attributes', kind='AttributeList', name_for_diagnostics='attributes',
                   collection_element_name='Attribute', is_optional=True),
             Child('Modifiers', kind='ModifierList', name_for_diagnostics='modifiers',
                   collection_element_name='Modifier', is_optional=True),
             Child('ImportTok', kind='ImportToken'),
             Child('ImportKind', kind='Token', is_optional=True,
                   token_choices=[
                      'TypealiasToken', 'StructToken', 'ClassToken',
                      'EnumToken', 'ProtocolToken', 'VarToken', 'LetToken',
                      'FuncToken',
                   ]),
             Child('Path', kind='AccessPath',
                   collection_element_name='PathComponent'),
         ]),

    # (value)
    Node('AccessorParameter', name_for_diagnostics=None, kind='Syntax',
         traits=['Parenthesized'],
         children=[
             Child('LeftParen', kind='LeftParenToken'),
             Child('Name', kind='IdentifierToken', name_for_diagnostics='name'),
             Child('RightParen', kind='RightParenToken'),
         ]),

    Node('AccessorDecl', name_for_diagnostics='accessor', kind='Decl',
         children=[
             Child('Attributes', kind='AttributeList', name_for_diagnostics='attributes',
                   collection_element_name='Attribute', is_optional=True),
             Child('Modifier', kind='DeclModifier', name_for_diagnostics='modifiers', is_optional=True),
             Child('AccessorKind', kind='Token',
                   text_choices=[
                      'get', 'set', 'didSet', 'willSet', 'unsafeAddress',
                      'addressWithOwner', 'addressWithNativeOwner',
                      'unsafeMutableAddress',
                      'mutableAddressWithOwner',
                      'mutableAddressWithNativeOwner',
                      '_read', '_modify'
                   ]),
             Child('Parameter', name_for_diagnostics='parameter', kind='AccessorParameter', is_optional=True),
             Child('AsyncKeyword', kind='ContextualKeywordToken',
                   text_choices=['async'], is_optional=True),
             Child('ThrowsKeyword', kind='Token',
                   is_optional=True,
                   token_choices=[
                       'ThrowsToken',
                       'RethrowsToken',
                   ]),
             Child('Body', kind='CodeBlock', is_optional=True),
         ]),

    Node('AccessorList', name_for_diagnostics=None, kind="SyntaxCollection",
         element='AccessorDecl'),

    Node('AccessorBlock', name_for_diagnostics=None, kind="Syntax", traits=['Braced'],
         children=[
             Child('LeftBrace', kind='LeftBraceToken'),
             Child('Accessors', kind='AccessorList',
                   collection_element_name='Accessor'),
             Child('RightBrace', kind='RightBraceToken'),
         ]),

    # Pattern: Type = Value { get {} },
    Node('PatternBinding', name_for_diagnostics=None, kind="Syntax",
         traits=['WithTrailingComma'],
         children=[
             Child('Pattern', kind='Pattern'),
             Child('TypeAnnotation', kind='TypeAnnotation', name_for_diagnostics='type annotation', is_optional=True),
             Child('Initializer', kind='InitializerClause', is_optional=True),
             Child('Accessor', kind='Syntax', is_optional=True,
                   node_choices=[
                      Child('Accessors', kind='AccessorBlock'),
                      Child('Getter', kind='CodeBlock')]),
             Child('TrailingComma', kind='CommaToken', is_optional=True),
         ]),

    Node('PatternBindingList', name_for_diagnostics=None, kind="SyntaxCollection",
         element='PatternBinding'),

    Node('VariableDecl', name_for_diagnostics='variable', kind='Decl',
         children=[
             Child('Attributes', kind='AttributeList', name_for_diagnostics='attributes',
                   collection_element_name='Attribute', is_optional=True),
             Child('Modifiers', kind='ModifierList', name_for_diagnostics='modifiers',
                   collection_element_name='Modifier', is_optional=True),
             Child('LetOrVarKeyword', kind='Token',
                   token_choices=[
                       'LetToken', 'VarToken',
                   ]),
             Child('Bindings', kind='PatternBindingList',
                   collection_element_name='Binding'),
         ]),

    Node('EnumCaseElement', name_for_diagnostics=None, kind='Syntax',
         description='''
         An element of an enum case, containing the name of the case and,
         optionally, either associated values or an assignment to a raw value.
         ''',
         traits=['WithTrailingComma'],
         children=[
             Child('Identifier', kind='IdentifierToken', name_for_diagnostics='name',
                   description='The name of this case.'),
             Child('AssociatedValue', kind='ParameterClause', name_for_diagnostics='associated values', is_optional=True,
                   description='The set of associated values of the case.'),
             Child('RawValue', kind='InitializerClause', is_optional=True,
                   description='''
                   The raw value of this enum element, if present.
                   '''),
             Child('TrailingComma', kind='CommaToken', is_optional=True,
                   description='''
                   The trailing comma of this element, if the case has
                   multiple elements.
                   '''),
         ]),

    Node('EnumCaseElementList', name_for_diagnostics=None, kind='SyntaxCollection',
         description='A collection of 0 or more `EnumCaseElement`s.',
         element='EnumCaseElement'),

    Node('EnumCaseDecl', name_for_diagnostics='enum case', kind='Decl',
         description='''
         A `case` declaration of a Swift `enum`. It can have 1 or more
         `EnumCaseElement`s inside, each declaring a different case of the
         enum.
         ''',
         children=[
             Child('Attributes', kind='AttributeList', name_for_diagnostics='attributes',
                   collection_element_name='Attribute', is_optional=True,
                   description='''
                   The attributes applied to the case declaration.
                   '''),
             Child('Modifiers', kind='ModifierList', name_for_diagnostics='modifiers',
                   collection_element_name='Modifier', is_optional=True,
                   description='''
                   The declaration modifiers applied to the case declaration.
                   '''),
             Child('CaseKeyword', kind='CaseToken',
                   description='The `case` keyword for this case.'),
             Child('Elements', kind='EnumCaseElementList',
                   collection_element_name='Element', name_for_diagnostics='elements',
                   description='The elements this case declares.')
         ]),

    Node('EnumDecl', name_for_diagnostics='enum', kind='Decl',
         traits=['IdentifiedDecl'],
         description='A Swift `enum` declaration.',
         children=[
             Child('Attributes', kind='AttributeList', name_for_diagnostics='attributes',
                   collection_element_name='Attribute', is_optional=True,
                   description='''
                   The attributes applied to the enum declaration.
                   '''),
             Child('Modifiers', kind='ModifierList', name_for_diagnostics='modifiers',
                   collection_element_name='Modifier', is_optional=True,
                   description='''
                   The declaration modifiers applied to the enum declaration.
                   '''),
             Child('EnumKeyword', kind='EnumToken',
                   description='''
                   The `enum` keyword for this declaration.
                   '''),
             Child('Identifier', kind='IdentifierToken', name_for_diagnostics='name',
                   description='''
                   The name of this enum.
                   '''),
             Child('GenericParameters', kind='GenericParameterClause', name_for_diagnostics='generic parameter clause',
                   is_optional=True,
                   description='''
                   The generic parameters, if any, for this enum.
                   '''),
             Child('InheritanceClause', kind='TypeInheritanceClause', name_for_diagnostics='inheritance clause',
                   is_optional=True,
                   description='''
                   The inheritance clause describing conformances or raw
                   values for this enum.
                   '''),
             Child('GenericWhereClause', kind='GenericWhereClause', name_for_diagnostics='generic where clause',
                   is_optional=True,
                   description='''
                   The `where` clause that applies to the generic parameters of
                   this enum.
                   '''),
             Child('Members', kind='MemberDeclBlock',
                   description='''
                   The cases and other members of this enum.
                   ''')
         ]),

    # operator-decl -> attribute? modifiers? 'operator' operator
    Node('OperatorDecl', name_for_diagnostics='operator', kind='Decl',
         traits=['IdentifiedDecl'],
         description='A Swift `operator` declaration.',
         children=[
             Child('Attributes', kind='AttributeList', name_for_diagnostics='attributes',
                   collection_element_name='Attribute', is_optional=True,
                   description='''
                   The attributes applied to the 'operator' declaration.
                   '''),
             Child('Modifiers', kind='ModifierList', name_for_diagnostics='modifiers',
                   collection_element_name='Modifier', is_optional=True,
                   classification='Attribute',
                   description='''
                   The declaration modifiers applied to the 'operator'
                   declaration.
                   '''),
             Child('OperatorKeyword', kind='OperatorToken'),
             Child('Identifier', kind='Token', name_for_diagnostics='name',
                   token_choices=[
                       'UnspacedBinaryOperatorToken',
                       'SpacedBinaryOperatorToken',
                       'PrefixOperatorToken',
                       'PostfixOperatorToken',
                   ]),
             Child('OperatorPrecedenceAndTypes', kind='OperatorPrecedenceAndTypes',
                   description='''
                   Optionally specify a precedence group and designated types.
                   ''',
                   is_optional=True),
         ]),

    # designated-type-list -> (',' identifier)*
    Node('DesignatedTypeList', name_for_diagnostics=None, kind='SyntaxCollection',
         element='DesignatedTypeElement'),
    Node('DesignatedTypeElement', name_for_diagnostics=None, kind='Syntax',
         children=[
             Child('LeadingComma', kind='CommaToken'),
             Child('Name', kind='IdentifierToken'),
         ]),

    # infix-operator-group -> ':' identifier designated-type-list?
    Node('OperatorPrecedenceAndTypes', name_for_diagnostics=None, kind='Syntax',
         description='''
         A clause to specify precedence group in infix operator declarations, and designated types in any operator declaration.
         ''',
         children=[
             Child('Colon', kind='ColonToken'),
             Child('PrecedenceGroup', kind='IdentifierToken',
                   description='''
                   The precedence group for this operator
                   '''),
             Child('DesignatedTypes', kind='DesignatedTypeList',
                   collection_element_name='DesignatedTypeElement',
                   description='''
                   The designated types associated with this operator.
                   '''),
         ]),

    # precedence-group-decl -> attributes? modifiers? 'precedencegroup'
    #                            identifier '{' precedence-group-attribute-list
    #                            '}'
    Node('PrecedenceGroupDecl', name_for_diagnostics='precedencegroup', kind='Decl',
         traits=['IdentifiedDecl'],
         description='A Swift `precedencegroup` declaration.',
         children=[
             Child('Attributes', kind='AttributeList', name_for_diagnostics='attributes',
                   collection_element_name='Attribute', is_optional=True,
                   description='''
                   The attributes applied to the 'precedencegroup' declaration.
                   '''),
             Child('Modifiers', kind='ModifierList', name_for_diagnostics='modifiers',
                   collection_element_name='Modifier', is_optional=True,
                   description='''
                   The declaration modifiers applied to the 'precedencegroup'
                   declaration.
                   '''),
             Child('PrecedencegroupKeyword', kind='PrecedencegroupToken'),
             Child('Identifier', kind='IdentifierToken', name_for_diagnostics='name',
                   description='''
                   The name of this precedence group.
                   '''),
             Child('LeftBrace', kind='LeftBraceToken'),
             Child('GroupAttributes', kind='PrecedenceGroupAttributeList',
                   collection_element_name='GroupAttribute',
                   description='''
                   The characteristics of this precedence group.
                   '''),
             Child('RightBrace', kind='RightBraceToken'),
         ]),

    # precedence-group-attribute-list ->
    #     (precedence-group-relation | precedence-group-assignment |
    #      precedence-group-associativity )*
    Node('PrecedenceGroupAttributeList', name_for_diagnostics=None,
         kind='SyntaxCollection', element='Syntax',
         element_name='PrecedenceGroupAttribute',
         element_choices=[
             'PrecedenceGroupRelation',
             'PrecedenceGroupAssignment',
             'PrecedenceGroupAssociativity'
         ]),

    # precedence-group-relation ->
    #     ('higherThan' | 'lowerThan') ':' precedence-group-name-list
    Node('PrecedenceGroupRelation',
         name_for_diagnostics="'relation' property of precedencegroup", kind='Syntax',
         description='''
         Specify the new precedence group's relation to existing precedence
         groups.
         ''',
         children=[
             Child('HigherThanOrLowerThan', kind='IdentifierToken',
                   classification='Keyword',
                   text_choices=[
                      'higherThan', 'lowerThan',
                   ],
                   description='''
                   The relation to specified other precedence groups.
                   '''),
             Child('Colon', kind='ColonToken'),
             Child('OtherNames', kind='PrecedenceGroupNameList',
                   collection_element_name='OtherName',
                   description='''
                   The name of other precedence group to which this precedence
                   group relates.
                   '''),
         ]),

    # precedence-group-name-list ->
    #    identifier (',' identifier)*
    Node('PrecedenceGroupNameList', name_for_diagnostics=None, kind='SyntaxCollection',
         element='PrecedenceGroupNameElement'),
    Node('PrecedenceGroupNameElement', name_for_diagnostics=None, kind='Syntax',
         children=[
             Child('Name', kind='IdentifierToken', name_for_diagnostics='name'),
             Child('TrailingComma', kind='CommaToken',
                   is_optional=True),
         ]),

    # precedence-group-assignment ->
    #     'assignment' ':' ('true' | 'false')
    Node('PrecedenceGroupAssignment',
         name_for_diagnostics="'assignment' property of precedencegroup", kind='Syntax',
         description='''
         Specifies the precedence of an operator when used in an operation
         that includes optional chaining.
         ''',
         children=[
             Child('AssignmentKeyword', kind='IdentifierToken',
                   text_choices=['assignment']),
             Child('Colon', kind='ColonToken'),
             Child('Flag', kind='Token',
                   token_choices=[
                       'TrueToken',
                       'FalseToken',
                   ],
                   description='''
                   When true, an operator in the corresponding precedence group
                   uses the same grouping rules during optional chaining as the
                   assignment operators from the standard library. Otherwise,
                   operators in the precedence group follows the same optional
                   chaining rules as operators that don't perform assignment.
                   '''),
         ]),

    # precedence-group-associativity ->
    #     'associativity' ':' ('left' | 'right' | 'none')
    Node('PrecedenceGroupAssociativity',
         name_for_diagnostics="'associativity' property of precedencegroup",
         kind='Syntax',
         description='''
         Specifies how a sequence of operators with the same precedence level
         are grouped together in the absence of grouping parentheses.
         ''',
         children=[
             Child('AssociativityKeyword', kind='IdentifierToken',
                   classification='Keyword', text_choices=['associativity']),
             Child('Colon', kind='ColonToken'),
             Child('Value', kind='IdentifierToken',
                   text_choices=['left', 'right', 'none'],
                   description='''
                   Operators that are `left`-associative group left-to-right.
                   Operators that are `right`-associative group right-to-left.
                   Operators that are specified with an associativity of `none`
                   don't associate at all
                   '''),
         ]),
]
