
@available(watchOS 2.0, *)
class PKContact : NSObject {
  var name: NSPersonNameComponents?
  var postalAddress: CNPostalAddress?
  var emailAddress: String?
  var phoneNumber: CNPhoneNumber?
}
