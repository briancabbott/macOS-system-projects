
typealias CSSM_KRSP_HANDLE = uint32
struct cssm_kr_name {
  var Type: uint8
  var Length: uint8
  var Name: UnsafeMutablePointer<Int8>
  init()
  init(Type Type: uint8, Length Length: uint8, Name Name: UnsafeMutablePointer<Int8>)
}
struct cssm_kr_profile {
  var UserName: CSSM_KR_NAME
  var UserCertificate: CSSM_CERTGROUP_PTR
  var KRSCertChain: CSSM_CERTGROUP_PTR
  var LE_KRANum: uint8
  var LE_KRACertChainList: CSSM_CERTGROUP_PTR
  var ENT_KRANum: uint8
  var ENT_KRACertChainList: CSSM_CERTGROUP_PTR
  var INDIV_KRANum: uint8
  var INDIV_KRACertChainList: CSSM_CERTGROUP_PTR
  var INDIV_AuthenticationInfo: CSSM_DATA_PTR
  var KRSPFlags: uint32
  var KRSPExtensions: CSSM_DATA_PTR
  init()
  init(UserName UserName: CSSM_KR_NAME, UserCertificate UserCertificate: CSSM_CERTGROUP_PTR, KRSCertChain KRSCertChain: CSSM_CERTGROUP_PTR, LE_KRANum LE_KRANum: uint8, LE_KRACertChainList LE_KRACertChainList: CSSM_CERTGROUP_PTR, ENT_KRANum ENT_KRANum: uint8, ENT_KRACertChainList ENT_KRACertChainList: CSSM_CERTGROUP_PTR, INDIV_KRANum INDIV_KRANum: uint8, INDIV_KRACertChainList INDIV_KRACertChainList: CSSM_CERTGROUP_PTR, INDIV_AuthenticationInfo INDIV_AuthenticationInfo: CSSM_DATA_PTR, KRSPFlags KRSPFlags: uint32, KRSPExtensions KRSPExtensions: CSSM_DATA_PTR)
}
struct cssm_kr_wrappedproductinfo {
  var StandardVersion: CSSM_VERSION
  var StandardDescription: CSSM_STRING
  var ProductVersion: CSSM_VERSION
  var ProductDescription: CSSM_STRING
  var ProductVendor: CSSM_STRING
  var ProductFlags: uint32
  init()
  init(StandardVersion StandardVersion: CSSM_VERSION, StandardDescription StandardDescription: CSSM_STRING, ProductVersion ProductVersion: CSSM_VERSION, ProductDescription ProductDescription: CSSM_STRING, ProductVendor ProductVendor: CSSM_STRING, ProductFlags ProductFlags: uint32)
}
struct cssm_krsubservice {
  var SubServiceId: uint32
  var Description: UnsafeMutablePointer<Int8>
  var WrappedProduct: CSSM_KR_WRAPPEDPRODUCT_INFO
  init()
  init(SubServiceId SubServiceId: uint32, Description Description: UnsafeMutablePointer<Int8>, WrappedProduct WrappedProduct: CSSM_KR_WRAPPEDPRODUCT_INFO)
}
typealias CSSM_KRSUBSERVICE = cssm_krsubservice
typealias CSSM_KRSUBSERVICE_PTR = UnsafeMutablePointer<cssm_krsubservice>
typealias CSSM_KR_POLICY_TYPE = uint32
var CSSM_KR_INDIV_POLICY: Int32 { get }
var CSSM_KR_ENT_POLICY: Int32 { get }
var CSSM_KR_LE_MAN_POLICY: Int32 { get }
var CSSM_KR_LE_USE_POLICY: Int32 { get }
typealias CSSM_KR_POLICY_FLAGS = uint32
var CSSM_KR_INDIV: Int32 { get }
var CSSM_KR_ENT: Int32 { get }
var CSSM_KR_LE_MAN: Int32 { get }
var CSSM_KR_LE_USE: Int32 { get }
var CSSM_KR_OPTIMIZE: Int32 { get }
var CSSM_KR_DROP_WORKFACTOR: Int32 { get }
struct cssm_kr_policy_list_item {
  var next: COpaquePointer
  var AlgorithmId: CSSM_ALGORITHMS
  var Mode: CSSM_ENCRYPT_MODE
  var MaxKeyLength: uint32
  var MaxRounds: uint32
  var WorkFactor: uint8
  var PolicyFlags: CSSM_KR_POLICY_FLAGS
  var AlgClass: CSSM_CONTEXT_TYPE
  init()
}
struct cssm_kr_policy_info {
  var krbNotAllowed: CSSM_BOOL
  var numberOfEntries: uint32
  var policyEntry: UnsafeMutablePointer<CSSM_KR_POLICY_LIST_ITEM>
  init()
  init(krbNotAllowed krbNotAllowed: CSSM_BOOL, numberOfEntries numberOfEntries: uint32, policyEntry policyEntry: UnsafeMutablePointer<CSSM_KR_POLICY_LIST_ITEM>)
}
