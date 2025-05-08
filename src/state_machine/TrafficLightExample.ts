/**
 * Traffic Light Example using the Finite State Machine
 *
 * This example demonstrates how to use the FiniteStateMachine class
 * to model a simple traffic light with three states: red, yellow, and green.
 */

import { FiniteStateMachine } from "./FiniteStateMachine";

// Define the context type for our traffic light
interface TrafficLightContext {
  timeInState: number;
  crossingPedestrianCount: number;
}

// Define the events that can be sent to the traffic light
type TrafficLightEvent =
  | { type: "TIMER" }
  | { type: "EMERGENCY_VEHICLE" }
  | { type: "PEDESTRIAN_BUTTON"; count: number };

// Create the traffic light state machine
const trafficLightMachine = new FiniteStateMachine<
  TrafficLightContext,
  TrafficLightEvent
>({
  initial: "red",
  context: {
    timeInState: 0,
    crossingPedestrianCount: 0,
  },
  states: {
    red: {
      onEnter: (context) => {
        console.log("Traffic light turned RED. Stop all vehicles.");
        context.timeInState = 0;
      },
      transitions: {
        TIMER: {
          target: "green",
          cond: (context) => context.timeInState >= 30, // 30 seconds of red
          actions: (context) => {
            console.log("Red light timer completed");
            context.crossingPedestrianCount = 0;
          },
        },
        EMERGENCY_VEHICLE: {
          target: "green",
          actions: (context) => {
            console.log("Emergency vehicle detected, switching to green");
            context.crossingPedestrianCount = 0;
          },
        },
      },
    },
    yellow: {
      onEnter: (context) => {
        console.log("Traffic light turned YELLOW. Prepare to stop.");
        context.timeInState = 0;
      },
      transitions: {
        TIMER: {
          target: "red",
          cond: (context) => context.timeInState >= 5, // 5 seconds of yellow
          actions: (context) => {
            console.log("Yellow light timer completed");
          },
        },
        EMERGENCY_VEHICLE: {
          target: "red",
          actions: () => {
            console.log(
              "Emergency vehicle detected, switching to red immediately"
            );
          },
        },
      },
    },
    green: {
      onEnter: (context) => {
        console.log("Traffic light turned GREEN. Vehicles may proceed.");
        context.timeInState = 0;
      },
      transitions: {
        TIMER: {
          target: "yellow",
          cond: (context) => context.timeInState >= 45, // 45 seconds of green
          actions: (context) => {
            console.log("Green light timer completed");
          },
        },
        PEDESTRIAN_BUTTON: {
          target: "yellow",
          cond: (context, event) => {
            if (event.type !== "PEDESTRIAN_BUTTON") return false;

            const totalPedestrians =
              context.crossingPedestrianCount + event.count;
            return totalPedestrians > 5 && context.timeInState > 15; // Min 15s green and 5+ pedestrians
          },
          actions: (context, event) => {
            const pedestrianEvent = event as {
              type: "PEDESTRIAN_BUTTON";
              count: number;
            };
            console.log(
              `Pedestrian button pressed, ${pedestrianEvent.count} pedestrians waiting`
            );
            context.crossingPedestrianCount += pedestrianEvent.count;
          },
        },
        EMERGENCY_VEHICLE: {
          target: "yellow",
          actions: () => {
            console.log("Emergency vehicle detected, switching to yellow");
          },
        },
      },
    },
  },
});

// Example usage
export function simulateTrafficLight() {
  // Example usage: Start in red state
  console.log(`Current state: ${trafficLightMachine.getState()}`);

  // Simulate time passing (red light timer)
  trafficLightMachine.updateContext({ timeInState: 35 });
  trafficLightMachine.send({ type: "TIMER" });
  console.log(`Current state: ${trafficLightMachine.getState()}`);

  // Simulate pedestrian button press during green light
  trafficLightMachine.updateContext({ timeInState: 20 });
  trafficLightMachine.send({ type: "PEDESTRIAN_BUTTON", count: 6 });
  console.log(`Current state: ${trafficLightMachine.getState()}`);

  // Simulate yellow light timing out
  trafficLightMachine.updateContext({ timeInState: 10 });
  trafficLightMachine.send({ type: "TIMER" });
  console.log(`Current state: ${trafficLightMachine.getState()}`);

  // Simulate emergency vehicle during red light
  trafficLightMachine.send({ type: "EMERGENCY_VEHICLE" });
  console.log(`Current state: ${trafficLightMachine.getState()}`);

  // Log final context
  console.log("Final context:", trafficLightMachine.getContext());
}
