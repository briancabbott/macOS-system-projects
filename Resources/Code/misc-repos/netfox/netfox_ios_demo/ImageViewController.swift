//
//  ImageViewController.swift
//  netfox_ios_demo
//
//  Created by Nathan Jangula on 10/12/17.
//  Copyright © 2017 kasketis. All rights reserved.
//

import UIKit

class ImageViewController: UIViewController {

    @IBOutlet weak var imageView: UIImageView!
    var session: URLSession?
    var dataTask: URLSessionDataTask?
    
    required init?(coder aDecoder: NSCoder) {
        super.init(coder: aDecoder)
    }

    @IBAction func tappedLoadImage(_ sender: Any) {
        dataTask?.cancel()
        
        if session == nil {
            session = URLSession(configuration: URLSessionConfiguration.default)
        }
        
        if let url = URL(string: "https://picsum.photos/\(Int(imageView.frame.size.width))/\(Int(imageView.frame.size.height))") {
            dataTask = session?.dataTask(with: url, completionHandler: { (data, response, error) in
                if let error = error {
                    self.handleCompletion(error: error.localizedDescription, data: data)
                } else {
                    guard let data = data else { self.handleCompletion(error: "Invalid data", data: nil); return }
                    guard let response = response as? HTTPURLResponse else { self.handleCompletion(error: "Invalid response", data: data); return }
                    guard response.statusCode >= 200 && response.statusCode < 300 else { self.handleCompletion(error: "Invalid response code", data: data); return }
                    
                    self.handleCompletion(error: error?.localizedDescription, data: data)
                }
            })
            
            dataTask?.resume()
        }
    }
    
    private func handleCompletion(error: String?, data: Data?) {
        DispatchQueue.main.async {
            
            if let error = error {
                NSLog(error)
                return
            }
            
            if let data = data {
                let image = UIImage(data: data)
                NSLog("\(image?.size.width ?? 0),\(image?.size.height ?? 0)")
                self.imageView.image = image
            }
        }
    }
}

