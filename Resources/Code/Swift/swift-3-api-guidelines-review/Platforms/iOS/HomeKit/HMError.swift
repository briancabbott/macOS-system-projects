
@available(iOS 8.0, *)
let HMErrorDomain: String
@available(iOS 8.0, *)
enum HMErrorCode : Int {
  case alreadyExists
  case notFound
  case invalidParameter
  case accessoryNotReachable
  case readOnlyCharacteristic
  case writeOnlyCharacteristic
  case notificationNotSupported
  case operationTimedOut
  case accessoryPoweredOff
  case accessDenied
  case objectAssociatedToAnotherHome
  case objectNotAssociatedToAnyHome
  case objectAlreadyAssociatedToHome
  case accessoryIsBusy
  case operationInProgress
  case accessoryOutOfResources
  case insufficientPrivileges
  case accessoryPairingFailed
  case invalidDataFormatSpecified
  case nilParameter
  case unconfiguredParameter
  case invalidClass
  case operationCancelled
  case roomForHomeCannotBeInZone
  case noActionsInActionSet
  case noRegisteredActionSets
  case missingParameter
  case fireDateInPast
  case roomForHomeCannotBeUpdated
  case actionInAnotherActionSet
  case objectWithSimilarNameExistsInHome
  case homeWithSimilarNameExists
  case renameWithSimilarName
  case cannotRemoveNonBridgeAccessory
  case nameContainsProhibitedCharacters
  case nameDoesNotStartWithValidCharacters
  case userIDNotEmailAddress
  case userDeclinedAddingUser
  case userDeclinedRemovingUser
  case userDeclinedInvite
  case userManagementFailed
  case recurrenceTooSmall
  case invalidValueType
  case valueLowerThanMinimum
  case valueHigherThanMaximum
  case stringLongerThanMaximum
  case homeAccessNotAuthorized
  case operationNotSupported
  case maximumObjectLimitReached
  case accessorySentInvalidResponse
  case stringShorterThanMinimum
  case genericError
  case securityFailure
  case communicationFailure
  case messageAuthenticationFailed
  case invalidMessageSize
  case accessoryDiscoveryFailed
  case clientRequestError
  case accessoryResponseError
  case nameDoesNotEndWithValidCharacters
  case accessoryIsBlocked
  case invalidAssociatedServiceType
  case actionSetExecutionFailed
  case actionSetExecutionPartialSuccess
  case actionSetExecutionInProgress
  case accessoryOutOfCompliance
  case dataResetFailure
  case notificationAlreadyEnabled
  case recurrenceMustBeOnSpecifiedBoundaries
  case dateMustBeOnSpecifiedBoundaries
  case cannotActivateTriggerTooFarInFuture
  case recurrenceTooLarge
  case readWritePartialSuccess
  case readWriteFailure
  case notSignedIntoiCloud
  case keychainSyncNotEnabled
  case cloudDataSyncInProgress
  case networkUnavailable
  case addAccessoryFailed
  case missingEntitlement
  case cannotUnblockNonBridgeAccessory
  case deviceLocked
  @available(iOS 9.0, *)
  case cannotRemoveBuiltinActionSet
  @available(iOS 9.0, *)
  case locationForHomeDisabled
  @available(iOS 9.0, *)
  case notAuthorizedForLocationServices
}

extension HMErrorCode : _BridgedNSError {
}
