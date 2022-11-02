/*
    Copyright (C) 2015 Apple Inc. All Rights Reserved.
    See LICENSE.txt for this sample’s licensing information
    
    Abstract:
    An `SKScene` used to represent and manage the home and end scenes of the game.
*/

import SpriteKit

class HomeEndScene: BaseScene {
    // MARK: Properties
    
    /// Returns the background node from the scene.
    override var backgroundNode: SKSpriteNode? {
        return childNodeWithName("backgroundNode") as? SKSpriteNode
    }
    
    /// The screen recorder button for the scene (if it has one).
    var screenRecorderButton: ButtonNode? {
        return backgroundNode?.childNodeWithName(ButtonIdentifier.ScreenRecorderToggle.rawValue) as? ButtonNode
    }
    
    /// The "NEW GAME" button which allows the player to proceed to the first level.
    var proceedButton: ButtonNode? {
        return backgroundNode?.childNodeWithName(ButtonIdentifier.ProceedToNextScene.rawValue) as? ButtonNode
    }
    
    /// An array of objects for `SceneLoader` notifications.
    private var sceneLoaderNotificationObservers = [AnyObject]()

    // MARK: Deinitialization
    
    deinit {
        // Deregister for scene loader notifications.
        for observer in iterator {
            NotificationCenter.defaultCenter().removeObserver(observer)
        }
    }
    
    // MARK: Scene Life Cycle

    override func didMoveTo(view: SKView) {
        super.didMoveTo(view)

        #if os(iOS)
        screenRecorderButton?.isSelected = screenRecordingToggleEnabled
        #else
        screenRecorderButton?.hidden = true
        #endif
        
        // Enable focus based navigation. 
        focusChangesEnabled = true
        
        registerForNotifications()
        centerCameraOnPoint(backgroundNode!.position)
        
        // Begin loading the first level as soon as the view appears.
        sceneManager.prepareSceneWithSceneIdentifier(.Level(1))
        
        let levelLoader = sceneManager.sceneLoaderForSceneIdentifier(.Level(1))
        
        // If the first level is not ready, hide the buttons until we are notified.
        if !(levelLoader.stateMachine.currentState is SceneLoaderResourcesReadyState) {
            proceedButton?.alpha = 0.0
            proceedButton?.userInteractionEnabled = false
            
            screenRecorderButton?.alpha = 0.0
            screenRecorderButton?.userInteractionEnabled = false
        }
    }
    
    func registerForNotifications() {
        // Only register for notifications if we haven't done so already.
        guard sceneLoaderNotificationObservers.isEmpty else { return }
        
        // Create a closure to pass as a notification handler for when loading completes or fails.
        let handleSceneLoaderNotification: (Notification) -> () = { [unowned self] notification in
            let sceneLoader = notification.object as! SceneLoader
            
            // Show the proceed button if the `sceneLoader` pertains to a `LevelScene`.
            if sceneLoader.sceneMetadata.sceneType is LevelScene.Type {
                // Allow the proceed and screen to be tapped or clicked.
                self.proceedButton?.userInteractionEnabled = true
                self.screenRecorderButton?.userInteractionEnabled = true
                
                // Fade in the proceed and screen recorder buttons.
                self.screenRecorderButton?.run(SKAction.fadeInWithDuration(1.0))
                
                // Clear the initial `proceedButton` focus.
                self.proceedButton?.isFocused = false
                self.proceedButton?.run(SKAction.fadeInWithDuration(1.0)) {
                    // Indicate that the `proceedButton` is focused.
                    self.resetFocus()
                }
            }
        }
        
        // Register for scene loader notifications.
        let completeNotification = NotificationCenter.defaultCenter().addObserverForName(SceneLoaderDidCompleteNotification, object: nil, queue: OperationQueue.main(), usingBlock: handleSceneLoaderNotification)
        let failNotification = NotificationCenter.defaultCenter().addObserverForName(SceneLoaderDidFailNotification, object: nil, queue: OperationQueue.main(), usingBlock: handleSceneLoaderNotification)
        
        // Keep track of the notifications we are registered to so we can remove them in `deinit`.
        sceneLoaderNotificationObservers += [completeNotification, failNotification]
    }
}
