
@available(iOS 7.0, *)
func GSSCreateCredentialFromUUID(_ uuid: CFUUID) -> gss_cred_id_t
@available(iOS 8.0, *)
func GSSCreateError(_ mech: gss_const_OID, _ major_status: OM_uint32, _ minor_status: OM_uint32) -> Unmanaged<CFError>?
@available(iOS 7.0, *)
func GSSCreateName(_ name: CFTypeRef, _ name_type: gss_const_OID, _ error: UnsafeMutablePointer<Unmanaged<CFError>?>) -> gss_name_t
@available(iOS 7.0, *)
func GSSCredentialCopyName(_ cred: gss_cred_id_t) -> gss_name_t
@available(iOS 7.0, *)
func GSSCredentialCopyUUID(_ credential: gss_cred_id_t) -> Unmanaged<CFUUID>?
@available(iOS 7.0, *)
func GSSCredentialGetLifetime(_ cred: gss_cred_id_t) -> OM_uint32
@available(iOS 7.0, *)
func GSSNameCreateDisplayString(_ name: gss_name_t) -> Unmanaged<CFString>?
@available(iOS 6.0, *)
func gss_aapl_change_password(_ name: gss_name_t, _ mech: gss_const_OID, _ attributes: CFDictionary, _ error: UnsafeMutablePointer<Unmanaged<CFError>?>) -> OM_uint32
@available(iOS 5.0, *)
func gss_aapl_initial_cred(_ desired_name: gss_name_t, _ desired_mech: gss_const_OID, _ attributes: CFDictionary?, _ output_cred_handle: UnsafeMutablePointer<gss_cred_id_t>, _ error: UnsafeMutablePointer<Unmanaged<CFError>?>) -> OM_uint32
