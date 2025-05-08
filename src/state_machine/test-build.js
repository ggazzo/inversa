/**
 * Test script to run after bundle is built
 *
 * This script imports the built CommonJS bundle and runs a simple test
 * to verify that the build process worked correctly.
 */

// Load the bundled module
const { FiniteStateMachine, simulateTrafficLight } = require("./dist/index.js");

console.log("Testing FiniteStateMachine bundle:");
console.log("----------------------------------");

// Test the core FSM
const simpleMachine = new FiniteStateMachine({
  initial: "idle",
  context: { counter: 0 },
  states: {
    idle: {
      transitions: {
        START: { target: "running" },
      },
    },
    running: {
      onEnter: (ctx) => {
        ctx.counter++;
      },
      transitions: {
        STOP: { target: "idle" },
      },
    },
  },
});

console.log("Initial state:", simpleMachine.getState());
console.log("Initial context:", simpleMachine.getContext());

simpleMachine.send({ type: "START" });
console.log("After START event:", simpleMachine.getState());
console.log("Updated context:", simpleMachine.getContext());

simpleMachine.send({ type: "STOP" });
console.log("After STOP event:", simpleMachine.getState());

console.log("\nRunning TrafficLight example:");
console.log("---------------------------");
simulateTrafficLight();

console.log("\nAll tests completed successfully!");
