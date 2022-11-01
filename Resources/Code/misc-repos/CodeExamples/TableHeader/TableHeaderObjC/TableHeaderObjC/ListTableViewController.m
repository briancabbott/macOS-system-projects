//
//  ListTableViewController.m
//  TableHeaderObjC
//
//  Created by Keith Harrison http://useyourloaf.com
//  Copyright (c) 2017 Keith Harrison. All rights reserved.
//
//  Redistribution and use in source and binary forms, with or without
//  modification, are permitted provided that the following conditions are met:
//
//  1. Redistributions of source code must retain the above copyright
//  notice, this list of conditions and the following disclaimer.
//
//  2. Redistributions in binary form must reproduce the above copyright
//  notice, this list of conditions and the following disclaimer in the
//  documentation and/or other materials provided with the distribution.
//
//  3. Neither the name of the copyright holder nor the names of its
//  contributors may be used to endorse or promote products derived from
//  this software without specific prior written permission.
//
//  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
//  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
//  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
//  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
//  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
//  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
//  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
//  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
//  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
//  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
//  POSSIBILITY OF SUCH DAMAGE.


#import "ListTableViewController.h"
#import "ListDataSource.h"

@interface ListTableViewController ()
@property (nonatomic,strong) ListDataSource *listDataSource;
@end

@implementation ListTableViewController

- (void)viewDidLoad {
    [super viewDidLoad];
    self.listDataSource = [[ListDataSource alloc] init:self.tableView];
    self.tableView.dataSource = self.listDataSource;
    self.tableView.rowHeight = UITableViewAutomaticDimension;
    self.tableView.estimatedRowHeight = UITableViewAutomaticDimension;

    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(contentSizeDidChange:) name:UIContentSizeCategoryDidChangeNotification object:nil];
}

- (void)contentSizeDidChange:(NSNotification *)notification {
    [self.tableView reloadData];
}

// viewDidLayoutSubviews()
// Called to notify the view controller that its view has just laid out its
// subviews.

// When the bounds change for a view controller's view, the view adjusts
// the positions of its subviews and then the system calls this method.
// However, this method being called does not indicate that the individual
// layouts of the view's subviews have been adjusted. Each subview is
// responsible for adjusting its own layout.

// Your view controller can override this method to make changes after the
// view lays out its subviews.

// The default implementation of this method does nothing.

- (void)viewDidLayoutSubviews {
    [super viewDidLayoutSubviews];

    UIView *headerView = self.tableView.tableHeaderView;
    if (headerView != nil) {

        // The table view header is created with the frame size set in
        // the Storyboard. Calculate the new size and reset the header
        // view to trigger the layout.

        // Calculate the minimum height of the header view that allows
        // the text label to fit its preferred width.

        CGSize size = [headerView systemLayoutSizeFittingSize:UILayoutFittingCompressedSize];

        if (headerView.frame.size.height != size.height) {
            CGRect frame = headerView.frame;
            frame.size.height = size.height;
            headerView.frame = frame;

            // Need to set the header view property of the table view
            // to trigger the new layout. Be careful to only do this
            // once when the height changes or we get stuck in a layout loop.

            self.tableView.tableHeaderView = headerView;

            // Now that the table view header is sized correctly have
            // the table view redo its layout so that the cells are
            // correcly positioned for the new header size.

            // This only seems to be necessary on iOS 9.

            [self.tableView layoutIfNeeded];
        }
    }
}

@end
