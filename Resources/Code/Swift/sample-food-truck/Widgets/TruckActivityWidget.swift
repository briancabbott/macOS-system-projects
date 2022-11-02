/*
See LICENSE folder for this sample’s licensing information.

Abstract:
Defines the live activity and dynamic island.
*/

#if canImport(ActivityKit)
import SwiftUI
import WidgetKit
import FoodTruckKit

struct TruckActivityWidget: Widget {
    
    var body: some WidgetConfiguration {
        ActivityConfiguration(for: TruckActivityAttributes.self) { context in
            LiveActivityView(orderNumber: context.attributes.orderID, timerRange: context.state.timerRange)
                .widgetURL(URL(string: "foodtruck://order/\(context.attributes.orderID)"))
        } dynamicIsland: { context in
            DynamicIsland {
                DynamicIslandExpandedRegion(.leading) {
                    ExpandedLeadingView()
                }

                DynamicIslandExpandedRegion(.trailing, priority: 1) {
                    ExpandedTrailingView(orderNumber: context.attributes.orderID, timerInterval: context.state.timerRange)
                        .dynamicIsland(verticalPlacement: .belowIfTooWide)
                }
            } compactLeading: {
                Image("IslandCompactIcon")
                    .padding(4)
                    .background(.indigo.gradient, in: ContainerRelativeShape())
                   
            } compactTrailing: {
                Text(timerInterval: context.state.timerRange, countsDown: true)
                    .monospacedDigit()
                    .foregroundColor(Color("LightIndigo"))
                    .frame(width: 40)
            } minimal: {
                Image("IslandCompactIcon")
                    .padding(4)
                    .background(.indigo.gradient, in: ContainerRelativeShape())
            }
            .contentMargins(.trailing, 32, for: .expanded)
            .contentMargins([.leading, .top, .bottom], 6, for: .compactLeading)
            .contentMargins(.all, 6, for: .minimal)
            .widgetURL(URL(string: "foodtruck://order/\(context.attributes.orderID)"))
        }
    }
}

struct LiveActivityView: View {
    var orderNumber: String
    var timerRange: ClosedRange<Date>
    
    var body: some View {
        HStack {
            Image("IslandExpandedIcon")
                .clipShape(RoundedRectangle(cornerRadius: 12, style: .continuous))
            OrderInfoView(orderNumber: orderNumber)
            Spacer()
            OrderTimerView(timerRange: timerRange)
        }
        .tint(.primary)
        .padding([.leading, .top, .bottom])
        .padding(.trailing, 32)
        .activityBackgroundTint(Color("LiveActivityBackground"))
        .activitySystemActionForegroundColor(.primary)
    }
}

struct ExpandedLeadingView: View {
    var body: some View {
        Image("IslandExpandedIcon")
            .clipShape(ContainerRelativeShape())
    }
}

struct OrderInfoView: View {
    var orderNumber: String
    
    var body: some View {
        VStack(alignment: .leading, spacing: 0) {
            Text("Order \(orderNumber)")
                .font(.title3)
                .fontWeight(.semibold)
                .foregroundStyle(.tint)
            
            Text("6 donuts")
                .font(.subheadline)
                .fontWeight(.medium)
                .foregroundStyle(.secondary)
        }
    }
}

struct OrderTimerView: View {
    var timerRange: ClosedRange<Date>
    
    var body: some View {
        VStack(alignment: .trailing) {
            Text(timerInterval: timerRange, countsDown: true)
                .multilineTextAlignment(.trailing)
                .frame(width: 80)
                .font(.title3)
                .fontWeight(.semibold)
                .foregroundStyle(.tint)
            Text("Remaining")
                .font(.subheadline)
                .fontWeight(.medium)
                .foregroundStyle(.secondary)
        }
    }
}

struct ExpandedTrailingView: View {
    var orderNumber: String
    var timerInterval: ClosedRange<Date>
    
    var body: some View {
        HStack(alignment: .lastTextBaseline) {
            OrderInfoView(orderNumber: orderNumber)
            Spacer()
            OrderTimerView(timerRange: timerInterval)
        }
        .tint(Color("LightIndigo"))
    }
}
#endif
