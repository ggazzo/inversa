# TypeScript Finite State Machine

A lightweight, flexible, and type-safe implementation of a Finite State Machine (FSM) in TypeScript.

## Features

- **Type Safety**: Full TypeScript support with generics for context and events
- **Guard Conditions**: Conditional transitions based on the current context
- **Actions**: Run code on enter, exit, and during transitions
- **Context**: Maintain and update state across the machine
- **Simple API**: Easy to understand and use
- **Multiple Bundle Formats**: CommonJS, ESM, and UMD bundles via Rollup

## Installation

```bash
# Install dependencies
npm install

# Build the library
npm run build
```

## Build Options

This project uses Rollup to generate multiple bundle formats:

```bash
# Development mode with watch
npm run dev

# Production build
npm run build

# Test the bundle
npm run test:bundle

# Clean the dist directory
npm run clean
```

The build process generates:

- **CommonJS**: `/dist/index.js` - For Node.js and CommonJS environments
- **ES Module**: `/dist/index.esm.js` - For modern bundlers (webpack, Rollup, etc.)
- **UMD**: `/dist/index.umd.min.js` - For direct browser usage via script tag

## Basic Example: Traffic Light

```typescript
// Create a traffic light state machine
const trafficLight = new FiniteStateMachine({
  initial: "red",
  context: { timeInState: 0 },
  states: {
    red: {
      transitions: {
        TIMER: { target: "green" },
      },
    },
    yellow: {
      transitions: {
        TIMER: { target: "red" },
      },
    },
    green: {
      transitions: {
        TIMER: { target: "yellow" },
      },
    },
  },
});

// Change state
trafficLight.send({ type: "TIMER" });
console.log(trafficLight.getState()); // 'green'
```

## Advanced Example: Document Editor

A more complex example demonstrating a document editor with authentication, editing, and saving states can be found in `DocumentEditorExample.ts`.

## Core Concepts

1. **States**: The machine can be in exactly one state at any given time
2. **Events**: Triggers that can cause state transitions
3. **Transitions**: Rules that determine which state to move to when an event occurs
4. **Context**: Data that persists across state changes
5. **Actions**: Code that runs on state entry/exit or during transitions
6. **Guards**: Conditions that must be true for a transition to occur

## API Reference

### `FiniteStateMachine<TContext, TEvent>`

Main class for creating and interacting with state machines.

#### Methods

- `send(event: TEvent)`: Send an event to the machine to trigger a transition
- `getState()`: Get the current state
- `getContext()`: Get the current context
- `updateContext(update: Partial<TContext>)`: Update the context
- `matches(state: string)`: Check if the machine is in a specific state

## Contributing

1. Fork the repository
2. Create a feature branch: `git checkout -b feature/your-feature`
3. Commit your changes: `git commit -m 'Add your feature'`
4. Push to the branch: `git push origin feature/your-feature`
5. Submit a pull request

## License

MIT
