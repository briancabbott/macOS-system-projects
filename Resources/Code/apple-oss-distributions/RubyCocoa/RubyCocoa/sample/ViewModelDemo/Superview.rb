
require 'osx/cocoa'

include OSX

class MySuperview < NSView
		
	ib_outlets :controller, :shadowSwitch, :moveToTopSwitch
	
	attr_reader :shadowSwitch, :moveToTopSwitch

  def initWithFrame (frame)
    super_initWithFrame(frame)
    return self
  end

	def awakeFromNib
		@timer = nil
	end

  def drawRect(rect)
		# Draw the background only since the ViewModel objects (subviews) will
		# draw themselves automatically.
		# An appropriate rect passed in to this method will keep objects from
		# unnecessarily drawing themselves.
		NSColor.whiteColor.set
		NSBezierPath.fillRect(rect)
  end
	
	def shadowSwitchAction
		setNeedsDisplay true
	end
	
	def mouseDown(event)
		puts "MySuperview clicked."
	end
	
	def moveSubviewToTop(mySubview)
		# Moves the given subview to the top, for drawing order purposes
		addSubview_positioned_relativeTo_(mySubview, NSWindowBelow, subviews.lastObject)
	end
	
	def moveSubviewToIndex(mySubview, i)
		# An index of 0 will move it behind all others
		addSubview_positioned_relativeTo_(mySubview, NSWindowBelow, subviews.objectAtIndex(i))
	end
	
	def startTimer
		if @timer == nil
			@timer = NSTimer.scheduledTimerWithTimeInterval_target_selector_userInfo_repeats_(
				1.0/60.0, self, :timerAction,	nil, true)
			puts "startTimer"
		end
	end
	
	def timerAction
		numMoving = 0
		subviews.each do |sv|
			# The move function returns 0 if object is not moving, 1 otherwise
			numMoving += sv.moveToDestination
		end
		
		if numMoving == 0
			endTimer
			$objMoving = false
			return
		end
		
		setNeedsDisplay true
		#puts "objs moving = #{numMoving}"
	end
	
	def endTimer
		puts "endTimer"
		@timer.invalidate
		@timer = nil
	end
end
