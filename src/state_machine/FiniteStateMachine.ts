/**
 * Finite State Machine implementation in TypeScript
 *
 * This implementation provides a flexible and type-safe way to create and
 * manage state machines with strong typing support for states, events, and context.
 */

// Define types for the state machine
export type StateConfig<TContext, TEvent> = {
  // Actions that run when entering this state
  onEnter?: (context: TContext, event?: TEvent) => void;
  // Actions that run when exiting this state
  onExit?: (context: TContext, event?: TEvent) => void;
  // Transitions to other states
  transitions: {
    [eventType: string]:
      | {
          target: string;
          // Guard condition that must be true for the transition to occur
          cond?: (context: TContext, event: TEvent) => boolean;
          // Actions to run during the transition
          actions?: (context: TContext, event: TEvent) => void;
        }
      | Array<{
          target: string;
          // Guard condition that must be true for the transition to occur
          cond?: (context: TContext, event: TEvent) => boolean;
          // Actions to run during the transition
          actions?: (context: TContext, event: TEvent) => void;
        }>;
  };
};

export type MachineConfig<TContext, TEvent> = {
  // The initial state of the machine
  initial: string;
  // The context (data) shared across all states
  context: TContext;
  // The states of the machine
  states: {
    [state: string]: StateConfig<TContext, TEvent>;
  };
};

export class FiniteStateMachine<
  TContext extends Record<string, any> = Record<string, any>,
  TEvent extends { type: string } = { type: string }
> {
  private currentState: string;
  private config: MachineConfig<TContext, TEvent>;
  private context: TContext;

  /**
   * Create a new finite state machine
   * @param config The configuration for the state machine
   */
  constructor(config: MachineConfig<TContext, TEvent>) {
    this.config = config;
    this.context = { ...config.context };
    this.currentState = config.initial;

    // Run onEnter for the initial state if it exists
    const initialState = this.config.states[this.currentState];
    if (initialState?.onEnter) {
      initialState.onEnter(this.context);
    }
  }

  /**
   * Send an event to the state machine to trigger a transition
   * @param event The event to send
   * @returns The current state after processing the event
   */
  public send(event: TEvent): string {
    const currentStateConfig = this.config.states[this.currentState];

    // Check if there's a transition for this event
    const transition = currentStateConfig.transitions[event.type];

    if (!transition) {
      console.warn(
        `No transition found for event ${event.type} in state ${this.currentState}`
      );
      return this.currentState;
    }

    // Handle array of transitions (for multiple possible targets with different conditions)
    if (Array.isArray(transition)) {
      for (const t of transition) {
        // If there's a condition and it's not met, skip this transition
        if (t.cond && !t.cond(this.context, event)) {
          continue;
        }

        return this.executeTransition(t, event);
      }
      return this.currentState;
    }

    // Handle single transition
    // Check if the guard condition passes (if there is one)
    if (transition.cond && !transition.cond(this.context, event)) {
      return this.currentState;
    }

    return this.executeTransition(transition, event);
  }

  /**
   * Execute a transition from the current state to a new state
   * @param transition The transition to execute
   * @param event The event that triggered the transition
   * @returns The new current state
   */
  private executeTransition(
    transition: {
      target: string;
      cond?: (context: TContext, event: TEvent) => boolean;
      actions?: (context: TContext, event: TEvent) => void;
    },
    event: TEvent
  ): string {
    const currentStateConfig = this.config.states[this.currentState];

    // Run exit actions for current state
    if (currentStateConfig.onExit) {
      currentStateConfig.onExit(this.context, event);
    }

    // Run transition actions
    if (transition.actions) {
      transition.actions(this.context, event);
    }

    // Update current state
    const previousState = this.currentState;
    this.currentState = transition.target;

    // Run entry actions for new state
    const nextStateConfig = this.config.states[this.currentState];
    if (nextStateConfig.onEnter) {
      nextStateConfig.onEnter(this.context, event);
    }

    console.log(
      `Transition: ${previousState} -> ${this.currentState} (${event.type})`
    );

    return this.currentState;
  }

  /**
   * Get the current state of the machine
   */
  public getState(): string {
    return this.currentState;
  }

  /**
   * Get the current context of the machine
   */
  public getContext(): TContext {
    return { ...this.context };
  }

  /**
   * Update the context of the machine
   * @param contextUpdate A partial context object to merge with the current context
   */
  public updateContext(contextUpdate: Partial<TContext>): void {
    this.context = { ...this.context, ...contextUpdate };
  }

  /**
   * Check if the machine is in a specific state
   * @param state The state to check
   */
  public matches(state: string): boolean {
    return this.currentState === state;
  }
}
