from .Child import Child
from .Node import Node  # noqa: I201

GENERIC_NODES = [
    # generic-where-clause -> 'where' requirement-list
    Node('GenericWhereClause', name_for_diagnostics="'where' clause", kind='Syntax',
         children=[
             Child('WhereKeyword', kind='WhereToken'),
             Child('RequirementList', kind='GenericRequirementList',
                   collection_element_name='Requirement'),
         ]),

    Node('GenericRequirementList', name_for_diagnostics=None, kind='SyntaxCollection',
         element='GenericRequirement',
         element_name='GenericRequirement'),

    # generic-requirement ->
    #     (same-type-requirement|conformance-requirement|layout-requirement) ','?
    Node('GenericRequirement', name_for_diagnostics=None, kind='Syntax',
         traits=['WithTrailingComma'],
         children=[
             Child('Body', kind='Syntax',
                   node_choices=[
                       Child('SameTypeRequirement',
                             kind='SameTypeRequirement'),
                       Child('ConformanceRequirement',
                             kind='ConformanceRequirement'),
                       Child('LayoutRequirement',
                             kind='LayoutRequirement'),
                   ]),
             Child('TrailingComma', kind='CommaToken',
                   is_optional=True),
         ]),

    # same-type-requirement -> type-identifier == type
    Node('SameTypeRequirement', name_for_diagnostics='same type requirement',
         kind='Syntax',
         children=[
             Child('LeftTypeIdentifier', kind='Type', name_for_diagnostics='left-hand type'),
             Child('EqualityToken', kind='Token',
                   token_choices=[
                       'SpacedBinaryOperatorToken',
                       'UnspacedBinaryOperatorToken',
                       'PrefixOperatorToken',
                       'PostfixOperatorToken',
                   ]),
             Child('RightTypeIdentifier', name_for_diagnostics='right-hand type', kind='Type'),
         ]),

    # layout-requirement -> type-name : layout-constraint
    # layout-constraint -> identifier '('? integer-literal? ','? integer-literal? ')'?
    Node('LayoutRequirement', name_for_diagnostics='layout requirement', kind='Syntax',
         children=[
             Child('TypeIdentifier', kind='Type', name_for_diagnostics='constrained type'),
             Child('Colon', kind='ColonToken'),
             Child('LayoutConstraint', kind='IdentifierToken'),
             Child('LeftParen', kind='LeftParenToken',
                   is_optional=True),
             Child('Size', kind='IntegerLiteralToken', name_for_diagnostics='size', is_optional=True),
             Child('Comma', kind='CommaToken',
                   is_optional=True),
             Child('Alignment', kind='IntegerLiteralToken', name_for_diagnostics='alignment', is_optional=True),
             Child('RightParen', kind='RightParenToken',
                   is_optional=True),
         ]),

    Node('GenericParameterList', name_for_diagnostics=None, kind='SyntaxCollection',
         element='GenericParameter'),

    # generic-parameter -> type-name
    #                    | type-name : type-identifier
    #                    | type-name : protocol-composition-type
    Node('GenericParameter', name_for_diagnostics='generic parameter', kind='Syntax',
         traits=['WithTrailingComma'],
         children=[
             Child('Attributes', kind='AttributeList',
                   collection_element_name='Attribute', is_optional=True),
             Child('Name', name_for_diagnostics='name', kind='IdentifierToken'),
             Child('Colon', kind='ColonToken',
                   is_optional=True),
             Child('InheritedType', name_for_diagnostics='inherited type', kind='Type',
                   is_optional=True),
             Child('TrailingComma', kind='CommaToken',
                   is_optional=True),
         ]),

    Node('PrimaryAssociatedTypeList', name_for_diagnostics=None,
         kind='SyntaxCollection', element='PrimaryAssociatedType'),

    # primary-associated-type -> type-name ','?
    Node('PrimaryAssociatedType', name_for_diagnostics=None, kind='Syntax',
         traits=['WithTrailingComma'],
         children=[
             Child('Name', name_for_diagnostics='name', kind='IdentifierToken'),
             Child('TrailingComma', kind='CommaToken',
                   is_optional=True),
         ]),

    # generic-parameter-clause -> '<' generic-parameter-list generic-where-clause? '>'
    Node('GenericParameterClause', name_for_diagnostics='generic parameter clause',
         kind='Syntax',
         children=[
             Child('LeftAngleBracket', kind='LeftAngleToken'),
             Child('GenericParameterList', kind='GenericParameterList',
                   collection_element_name='GenericParameter'),
             Child('GenericWhereClause', kind='GenericWhereClause',
                   is_optional=True),
             Child('RightAngleBracket', kind='RightAngleToken'),
         ]),

    # conformance-requirement -> type-identifier : type-identifier
    Node('ConformanceRequirement', name_for_diagnostics='conformance requirement',
         kind='Syntax',
         children=[
             Child('LeftTypeIdentifier', kind='Type'),
             Child('Colon', kind='ColonToken'),
             Child('RightTypeIdentifier', kind='Type'),
         ]),

    # primary-associated-type-clause -> '<' primary-associated-type-list '>'
    Node('PrimaryAssociatedTypeClause',
         name_for_diagnostics='primary associated type clause', kind='Syntax',
         children=[
             Child('LeftAngleBracket', kind='LeftAngleToken'),
             Child('PrimaryAssociatedTypeList', kind='PrimaryAssociatedTypeList',
                   collection_element_name='PrimaryAssociatedType'),
             Child('RightAngleBracket', kind='RightAngleToken'),
         ]),
]
