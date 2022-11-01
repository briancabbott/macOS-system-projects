This example shows one possible way of implementing Drag and Drop for 
tableviews using bindings and core data. Our purpose is to provide a simple UI 
for adding members from a pool of all people into a club. The focus of this 
example is the ``NSObject`` subclass named ``DragSupportDataSource``. All of 
the table views in the application UI are bound to an array controller but 
have their data source set to a single ``DragSupportDataSource``.

``NSTableView`` drag and drop methods are called on the table view's datasource.
Using infoForBinding API, the ``DragSupportDataSource`` can find out which 
arraycontroller the table view in the drag operation is bound to. Once the 
destination array controller is found, it's simple to perform the correct 
operations.

The data source methods implemented by the ``DragSupportDataSource`` return 
``None``/``0`` so that the normal bindings machinery will populate the table 
view with data. This may seem like a waste, but is a simple way of letting the 
``DragSupportDataSource`` do the work of registering the table views for 
dragging. See ``DragSupportDataSource.py`` for more information. 

Things to keep in mind:

* The drag and drop implementation assumes all controllers are working with 
  the same ``NSManagedObjectContext``

* Most of the code in the ``DragSupportDataSource`` is for error checking and 
  un/packing objects
	
