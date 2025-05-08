/**
 * Document Editor Example using the Finite State Machine
 *
 * This example demonstrates a more complex use case for the FiniteStateMachine class,
 * modeling a document editor with various states like idle, editing, saving, etc.
 */

import { FiniteStateMachine } from "./FiniteStateMachine";

// Define the context type for our document editor
interface DocumentEditorContext {
  document: {
    id: string;
    content: string;
    isDirty: boolean;
    lastSaved: Date | null;
  };
  user: {
    id: string;
    name: string;
    isAuthenticated: boolean;
    hasEditPermission: boolean;
  };
  ui: {
    isSidebarOpen: boolean;
    activeDialog: string | null;
    selectedText: string | null;
  };
}

// Define the events that can be sent to the document editor
type DocumentEditorEvent =
  | { type: "LOAD_DOCUMENT"; documentId: string }
  | { type: "EDIT"; changes: string }
  | { type: "SAVE" }
  | { type: "SAVE_COMPLETE"; success: boolean; error?: string }
  | { type: "TOGGLE_SIDEBAR" }
  | { type: "FORMAT_TEXT"; format: "bold" | "italic" | "underline" }
  | { type: "LOGOUT" }
  | { type: "LOGIN"; userId: string; userName: string }
  | { type: "CANCEL" }
  | { type: "TIMEOUT" };

// Create the document editor state machine
const documentEditorMachine = new FiniteStateMachine<
  DocumentEditorContext,
  DocumentEditorEvent
>({
  initial: "authentication",
  context: {
    document: {
      id: "",
      content: "",
      isDirty: false,
      lastSaved: null,
    },
    user: {
      id: "",
      name: "",
      isAuthenticated: false,
      hasEditPermission: false,
    },
    ui: {
      isSidebarOpen: false,
      activeDialog: null,
      selectedText: null,
    },
  },
  states: {
    authentication: {
      onEnter: (context) => {
        console.log("Entering authentication state");
        context.ui.activeDialog = "login";
      },
      onExit: (context) => {
        console.log("Exiting authentication state");
        context.ui.activeDialog = null;
      },
      transitions: {
        LOGIN: {
          target: "idle",
          actions: (context, event) => {
            context.user.id = event.userId;
            context.user.name = event.userName;
            context.user.isAuthenticated = true;
            context.user.hasEditPermission = true; // For this example, all users have edit permission
            console.log(`User ${event.userName} logged in successfully`);
          },
        },
      },
    },
    idle: {
      onEnter: (context) => {
        console.log("Document editor is idle and ready");
      },
      transitions: {
        LOAD_DOCUMENT: {
          target: "loading",
          actions: (context, event) => {
            console.log(`Starting to load document: ${event.documentId}`);
            context.document.id = event.documentId;
          },
        },
        EDIT: {
          target: "editing",
          cond: (context) => context.user.hasEditPermission,
          actions: (context, event) => {
            console.log("Starting to edit document");
            context.document.content = event.changes;
            context.document.isDirty = true;
          },
        },
        TOGGLE_SIDEBAR: {
          target: "idle", // Self-transition
          actions: (context) => {
            context.ui.isSidebarOpen = !context.ui.isSidebarOpen;
            console.log(
              `Sidebar is now ${context.ui.isSidebarOpen ? "open" : "closed"}`
            );
          },
        },
        LOGOUT: {
          target: "authentication",
          actions: (context) => {
            console.log("User logging out");
            context.user.isAuthenticated = false;
            context.user.id = "";
            context.user.name = "";
            context.document = {
              id: "",
              content: "",
              isDirty: false,
              lastSaved: null,
            };
          },
        },
      },
    },
    loading: {
      onEnter: (context) => {
        console.log(`Loading document ${context.document.id}...`);
        // In a real implementation, this would make an API call to fetch the document
      },
      transitions: {
        SAVE_COMPLETE: {
          target: "idle",
          cond: (_, event) => event.success,
          actions: (context, event) => {
            console.log(`Document ${context.document.id} loaded successfully`);
            // Simulate loaded content
            context.document.content = `This is the content of document ${context.document.id}`;
            context.document.isDirty = false;
          },
        },
        TIMEOUT: {
          target: "error",
          actions: (context) => {
            console.log(
              `Failed to load document ${context.document.id}: Timeout`
            );
          },
        },
        CANCEL: {
          target: "idle",
          actions: () => {
            console.log("Document loading cancelled");
          },
        },
      },
    },
    editing: {
      onEnter: (context) => {
        console.log("Editing document");
      },
      transitions: {
        EDIT: {
          target: "editing", // Self-transition
          actions: (context, event) => {
            console.log("Applying edits to document");
            context.document.content += event.changes;
            context.document.isDirty = true;
          },
        },
        FORMAT_TEXT: {
          target: "editing", // Self-transition
          cond: (context) => !!context.ui.selectedText,
          actions: (context, event) => {
            console.log(`Formatting selected text with ${event.format}`);
            // In a real implementation, this would apply formatting to the selected text
          },
        },
        SAVE: {
          target: "saving",
          cond: (context) => context.document.isDirty,
          actions: (context) => {
            console.log(`Saving document ${context.document.id}`);
          },
        },
        TOGGLE_SIDEBAR: {
          target: "editing", // Self-transition
          actions: (context) => {
            context.ui.isSidebarOpen = !context.ui.isSidebarOpen;
            console.log(
              `Sidebar is now ${context.ui.isSidebarOpen ? "open" : "closed"}`
            );
          },
        },
      },
    },
    saving: {
      onEnter: (context) => {
        console.log(`Saving document ${context.document.id}...`);
        // In a real implementation, this would make an API call to save the document
      },
      transitions: {
        SAVE_COMPLETE: [
          {
            target: "idle",
            cond: (_, event) => event.success,
            actions: (context) => {
              console.log(`Document ${context.document.id} saved successfully`);
              context.document.isDirty = false;
              context.document.lastSaved = new Date();
            },
          },
          {
            target: "error",
            cond: (_, event) => !event.success,
            actions: (_, event) => {
              console.log(`Failed to save document: ${event.error}`);
            },
          },
        ],
        TIMEOUT: {
          target: "error",
          actions: () => {
            console.log("Document save timeout");
          },
        },
        CANCEL: {
          target: "editing",
          actions: () => {
            console.log("Save operation cancelled");
          },
        },
      },
    },
    error: {
      onEnter: (context) => {
        console.log("Entered error state");
        context.ui.activeDialog = "error";
      },
      onExit: (context) => {
        context.ui.activeDialog = null;
      },
      transitions: {
        CANCEL: {
          target: "idle",
          actions: () => {
            console.log("Error acknowledged");
          },
        },
      },
    },
  },
});

// Example usage
function simulateDocumentEditor() {
  console.log(`Initial state: ${documentEditorMachine.getState()}`);

  // Log in
  documentEditorMachine.send({
    type: "LOGIN",
    userId: "user123",
    userName: "John Smith",
  });
  console.log(`After login: ${documentEditorMachine.getState()}`);

  // Load a document
  documentEditorMachine.send({
    type: "LOAD_DOCUMENT",
    documentId: "doc-456",
  });
  console.log(`After load request: ${documentEditorMachine.getState()}`);

  // Document loaded successfully
  documentEditorMachine.send({
    type: "SAVE_COMPLETE",
    success: true,
  });
  console.log(`After document loaded: ${documentEditorMachine.getState()}`);

  // Start editing
  documentEditorMachine.send({
    type: "EDIT",
    changes: " - edited by John",
  });
  console.log(`After edit: ${documentEditorMachine.getState()}`);

  // Toggle sidebar while editing
  documentEditorMachine.send({ type: "TOGGLE_SIDEBAR" });

  // Apply formatting
  documentEditorMachine.updateContext({
    ui: {
      ...documentEditorMachine.getContext().ui,
      selectedText: "John",
    },
  });
  documentEditorMachine.send({
    type: "FORMAT_TEXT",
    format: "bold",
  });

  // Save the document
  documentEditorMachine.send({ type: "SAVE" });
  console.log(`After save request: ${documentEditorMachine.getState()}`);

  // Save fails
  documentEditorMachine.send({
    type: "SAVE_COMPLETE",
    success: false,
    error: "Network error",
  });
  console.log(`After save error: ${documentEditorMachine.getState()}`);

  // Acknowledge error
  documentEditorMachine.send({ type: "CANCEL" });
  console.log(
    `After error acknowledgement: ${documentEditorMachine.getState()}`
  );

  // Log out
  documentEditorMachine.send({ type: "LOGOUT" });
  console.log(`After logout: ${documentEditorMachine.getState()}`);

  // Log final context
  console.log(
    "Final context:",
    JSON.stringify(documentEditorMachine.getContext(), null, 2)
  );
}

// Run the simulation
simulateDocumentEditor();
