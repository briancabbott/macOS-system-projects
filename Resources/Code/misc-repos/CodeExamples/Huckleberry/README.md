# Huckleberry

**This project is of historical interest only. I recommend using a self-sizing table view instead of the prototype cell approach used in this project.** 

## Auto layout table view cells with varying row heights

Example of how to implement table view cells with varying row heights
using auto layout constraints and a dummy prototype cell.

The fix for device rotation came from this GitHub example project:

https://github.com/smileyborg/TableViewCellWithAutoLayout

The source data for the table view is taken from the first 15 chapters
of Huckleberry Finn as found on Project Gutenberg. This gives nearly
2000 rows of data.

For further details see the following blog post:

+ [Table View Cells with Varying Row Heights](https://useyourloaf.com/blog/table-view-cells-with-varying-row-heights/)

The custom table view class (UYLTextCell) includes an implementation
of debugQuickLookObject to support Quick Look view of the cell in the
Xcode debugger (requires Xcode 5.1). See the following post for details:

+ [Xcode Debugger Quick Look](https://useyourloaf.com/blog/xcode-debugger-quick-look/)

## History

Version 1.2   17 March 2014      Fix device rotation  
Version 1.1   12 March 2014      Added Quick Look Debug  
Version 1.0   14 February 2014   Initial version.  
