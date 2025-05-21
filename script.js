document.addEventListener("DOMContentLoaded", () => {
  const connectButton = document.getElementById("connectButton");
  const statusArea = document.getElementById("statusArea");
  const currentTempDisplay = document.getElementById("currentTemp");
  const targetTempDisplay = document.getElementById("targetTempDisplay");
  const currentStateDisplay = document.getElementById("currentState");
  const confirmActionButton = document.getElementById("confirmActionButton");
  const rejectActionButton = document.getElementById("rejectActionButton");

  // New UI elements for SYNC data
  const kpDisplay = document.getElementById("kp");
  const kiDisplay = document.getElementById("ki");
  const kdDisplay = document.getElementById("kd");
  const hysteresisTempDisplay = document.getElementById("hysteresisTemp");
  const hysteresisTimeDisplay = document.getElementById("hysteresisTime");
  const volumeDisplay = document.getElementById("volume");
  const powerDisplay = document.getElementById("power");
  const tuningDisplay = document.getElementById("tuning");
  const confirmMessageDisplay = document.getElementById("confirmMessage");
  const messageDisplay = document.getElementById("message");
  const messageL2Display = document.getElementById("messageL2");
  const messageL3Display = document.getElementById("messageL3");

  // New UI elements for Subtask 15.2
  const setTargetTempInput = document.getElementById("setTargetTempInput");
  const setTargetTempButton = document.getElementById("setTargetTempButton");
  const startProcessButton = document.getElementById("startProcessButton");
  const stopProcessButton = document.getElementById("stopProcessButton");
  const getStatusButton = document.getElementById("getStatusButton");

  // Manual command elements
  const manualCommandInput = document.getElementById("manualCommandInput");
  const sendManualCommandButton = document.getElementById(
    "sendManualCommandButton"
  );
  const commandOutputArea = document.getElementById("commandOutputArea");

  // Auto-SYNC elements and state
  const autoSyncToggle = document.getElementById("autoSyncToggle");
  const syncIntervalInput = document.getElementById("syncIntervalInput");
  const clearCommandOutputButton = document.getElementById(
    "clearCommandOutputButton"
  );
  const listFilesButton = document.getElementById("listFilesButton");
  const fileListOutput = document.getElementById("fileListOutput");

  // Preferences UI
  const getPreferencesButton = document.getElementById("getPreferencesButton");
  const preferencesValuesDiv = document.getElementById("preferencesValues");
  const editablePreferencesContainer = document.getElementById(
    "editablePreferencesContainer"
  );
  const applyPreferencesButton = document.getElementById(
    "applyPreferencesButton"
  );

  // Input field references for editable preferences
  const prefSsidInput = document.getElementById("prefSsidInput");
  const prefPasswordInput = document.getElementById("prefPasswordInput");
  const prefVolumeInput = document.getElementById("prefVolumeInput");
  const prefPowerInput = document.getElementById("prefPowerInput");
  const prefHysteresisTempInput = document.getElementById(
    "prefHysteresisTempInput"
  );
  const prefHysteresisTimeInput = document.getElementById(
    "prefHysteresisTimeInput"
  );
  const prefPidKpInput = document.getElementById("prefPidKpInput");
  const prefPidKiInput = document.getElementById("prefPidKiInput");
  const prefPidKdInput = document.getElementById("prefPidKdInput");
  const prefPidPonInput = document.getElementById("prefPidPonInput");
  const prefPidSampleTimeInput = document.getElementById(
    "prefPidSampleTimeInput"
  );

  // The <details> element for preferences
  const preferencesSectionDetails =
    document.getElementById("preferencesSection");

  let autoSyncIntervalMs = 5000; // Default 5 seconds
  let isAutoSyncEnabled = false;
  let autoSyncTimerId = null;

  // Temperature Chart
  const tempCanvas = document.getElementById("temperatureChart");
  let temperatureChart;
  const maxDataPointsInput = document.getElementById("maxDataPointsInput");
  let configurableMaxDataPoints = 60; // Default value

  if (maxDataPointsInput) {
    configurableMaxDataPoints = parseInt(maxDataPointsInput.value, 10) || 60;
    maxDataPointsInput.addEventListener("change", (event) => {
      const newValue = parseInt(event.target.value, 10);
      if (!isNaN(newValue) && newValue >= 10 && newValue <= 1000) {
        // Basic validation
        configurableMaxDataPoints = newValue;
        console.log(
          `Max data points for graph set to: ${configurableMaxDataPoints}`
        );
        // The graph will adjust as new data comes in and old data is shifted out.
        // For an immediate effect, we might need to truncate/adjust existing chart data here,
        // but that adds complexity. For now, it affects future shifting.
      } else {
        event.target.value = configurableMaxDataPoints; // Reset to valid if input is bad
        console.warn("Invalid value for max data points. Reverted.");
      }
    });
  }

  // UUIDs - Updated for Nordic UART Service (NUS)
  const TARGET_SERVICE_UUID = "6e400001-b5a3-f393-e0a9-e50e24dcca9e";
  const COMMAND_CHARACTERISTIC_UUID = "6e400002-b5a3-f393-e0a9-e50e24dcca9e"; // NUS TX
  const TELEMETRY_CHARACTERISTIC_UUID = "6e400003-b5a3-f393-e0a9-e50e24dcca9e"; // NUS RX (for later)

  // Command IDs
  const COMMAND_SET_TARGET_TEMP = 0x01;
  const COMMAND_START_PROCESS = 0x02;
  const COMMAND_STOP_PROCESS = 0x03;
  const COMMAND_GET_STATUS = 0x04;
  const COMMAND_CONFIRM_ACTION = 0x05;
  const COMMAND_REJECT_ACTION = 0x06;

  let bleDevice;
  let bleServer;
  let commandCharacteristic; // Added for NUS TX characteristic
  let telemetryCharacteristic; // Added for NUS RX characteristic
  let lastMessageIdRequiringConfirmation = null; // To store message ID

  // Debounce for frequent UI updates (e.g., temperature)
  let uiUpdateTimeout = null;

  // Map for device states
  const DEVICE_STATES = {
    0: "Waiting Temperature",
    1: "Waiting Timer",
    2: "Waiting Confirmation",
    3: "PREPARING",
    4: "IDLE",
    // Add more states as defined by your device firmware
  };

  // For managing command IDs and callbacks
  let nextCommandId = 1;
  const pendingCommands = new Map();

  // Modal elements
  const confirmationModal = document.getElementById("confirmationModal");
  const modalConfirmMessage = document.getElementById("modalConfirmMessage");
  const modalConfirmBtn = document.getElementById("modalConfirmBtn");
  const modalRejectBtn = document.getElementById("modalRejectBtn");

  // State-specific timer display
  const stateTimerDisplayContainer = document.getElementById(
    "stateTimerDisplayContainer"
  );
  const stateTimerLabel = document.getElementById("stateTimerLabel");
  const stateTimerValue = document.getElementById("stateTimerValue");
  let stateCountdownIntervalId = null;

  // Store last known sample time
  let lastKnownSampleTime = 1000; // Default if not received

  // Helper function to append to command output area
  function appendToCommandOutput(message, type = "info") {
    if (!commandOutputArea) return;
    const timestamp = new Date().toLocaleTimeString();
    const line = document.createElement("div");
    line.textContent = `[${timestamp}] ${type.toUpperCase()}: ${message}`;
    if (type === "error") {
      line.style.color = "red";
    } else if (type === "response") {
      line.style.color = "blue";
    }
    commandOutputArea.appendChild(line);
    commandOutputArea.scrollTop = commandOutputArea.scrollHeight; // Auto-scroll
  }

  if (connectButton) {
    connectButton.onclick = async () => {
      if (!navigator.bluetooth) {
        statusArea.textContent =
          "Web Bluetooth API is not available in this browser.";
        console.warn("Web Bluetooth API not available.");
        return;
      }

      try {
        statusArea.textContent = "Requesting Bluetooth Device...";
        console.log("Requesting Bluetooth Device...");

        bleDevice = await navigator.bluetooth.requestDevice({
          optionalServices: ["6e400001-b5a3-f393-e0a9-e50e24dcca9e"],
          acceptAllDevices: true,
        });

        statusArea.textContent = `Connecting to ${
          bleDevice.name || bleDevice.id
        }...`;
        console.log(`Device selected: ${bleDevice.name || bleDevice.id}`);

        bleDevice.addEventListener("gattserverdisconnected", onDisconnected);

        bleServer = await bleDevice.gatt.connect();
        statusArea.textContent = "Connected to GATT Server. Getting Service...";
        console.log("Connected to GATT Server");

        const service = await bleServer.getPrimaryService(TARGET_SERVICE_UUID);
        statusArea.textContent =
          "Service Obtained. Getting Command Characteristic...";
        console.log("Service Obtained");

        commandCharacteristic = await service.getCharacteristic(
          COMMAND_CHARACTERISTIC_UUID
        );
        statusArea.textContent =
          "Command Characteristic Obtained. Getting Telemetry Characteristic...";
        console.log("Command Characteristic Obtained");

        telemetryCharacteristic = await service.getCharacteristic(
          TELEMETRY_CHARACTERISTIC_UUID
        );
        statusArea.textContent =
          "Telemetry Characteristic Obtained. Starting notifications...";
        console.log("Telemetry Characteristic Obtained");

        await telemetryCharacteristic.startNotifications();
        telemetryCharacteristic.addEventListener(
          "characteristicvaluechanged",
          handleTelemetry
        );
        statusArea.textContent = "Notifications started. Ready.";
        console.log("Telemetry notifications started.");

        connectButton.style.display = "none";
        rejectActionButton.style.display = "block";
        statusArea.textContent = `Connected to ${
          bleDevice.name || "device"
        }. Ready.`;
        console.log(
          `Connected to ${
            bleDevice.name || "device"
          } and notifications started.`
        );

        // TODO: Disable UI elements that require a connection
        currentTempDisplay.textContent = "N/A";
        targetTempDisplay.textContent = "N/A";
        currentStateDisplay.textContent = "N/A";

        // Reset new SYNC data displays
        if (kpDisplay) kpDisplay.textContent = "N/A";
        if (kiDisplay) kiDisplay.textContent = "N/A";
        if (kdDisplay) kdDisplay.textContent = "N/A";
        if (hysteresisTempDisplay) hysteresisTempDisplay.textContent = "N/A";
        if (hysteresisTimeDisplay) hysteresisTimeDisplay.textContent = "N/A";
        if (volumeDisplay) volumeDisplay.textContent = "N/A";
        if (powerDisplay) powerDisplay.textContent = "N/A";
        if (tuningDisplay) tuningDisplay.textContent = "N/A";
        if (confirmMessageDisplay) confirmMessageDisplay.textContent = "N/A";
        if (messageDisplay) messageDisplay.textContent = "N/A";
        if (messageL2Display) messageL2Display.textContent = "N/A";
        if (messageL3Display) messageL3Display.textContent = "N/A";

        clearStateTimer(); // Clear and hide state timer

        confirmActionButton.style.display = "none";
        rejectActionButton.style.display = "none";
        lastMessageIdRequiringConfirmation = null;
        if (isAutoSyncEnabled) {
          startAutoSync();
        }
      } catch (error) {
        statusArea.textContent = `Connection Failed: ${error.message}`;
        console.error("Connection failed:", error);
        bleDevice = null; // Reset device on error
        bleServer = null;
        commandCharacteristic = null;
        telemetryCharacteristic = null;
        if (bleDevice && bleDevice.gatt.connected) {
          bleDevice.gatt.disconnect();
        } else {
          onDisconnected();
        }
      }
    };
  }

  function onDisconnected(event) {
    const device = event.target;
    statusArea.textContent = `Device ${device.name || device.id} disconnected.`;
    console.log(`Device ${device.name || device.id} disconnected.`);
    connectButton.style.display = "block";
    confirmActionButton.style.display = "none";
    rejectActionButton.style.display = "none";
    bleDevice = null;
    bleServer = null;
    commandCharacteristic = null;
    telemetryCharacteristic = null;
    // TODO: Disable UI elements that require a connection
    currentTempDisplay.textContent = "N/A";
    targetTempDisplay.textContent = "N/A";
    currentStateDisplay.textContent = "N/A";

    // Reset new SYNC data displays on disconnect
    if (kpDisplay) kpDisplay.textContent = "N/A";
    if (kiDisplay) kiDisplay.textContent = "N/A";
    if (kdDisplay) kdDisplay.textContent = "N/A";
    if (hysteresisTempDisplay) hysteresisTempDisplay.textContent = "N/A";
    if (hysteresisTimeDisplay) hysteresisTimeDisplay.textContent = "N/A";
    if (volumeDisplay) volumeDisplay.textContent = "N/A";
    if (powerDisplay) powerDisplay.textContent = "N/A";
    if (tuningDisplay) tuningDisplay.textContent = "N/A";
    if (confirmMessageDisplay) confirmMessageDisplay.textContent = "N/A";
    if (messageDisplay) messageDisplay.textContent = "N/A";
    if (messageL2Display) messageL2Display.textContent = "N/A";
    if (messageL3Display) messageL3Display.textContent = "N/A";

    clearStateTimer(); // Clear and hide state timer on disconnect

    confirmActionButton.style.display = "none";
    rejectActionButton.style.display = "none";
    lastMessageIdRequiringConfirmation = null;
    stopAutoSync();
  }

  async function disconnectDevice() {
    if (!bleDevice) {
      console.log("No device connected.");
      return;
    }
    if (bleDevice.gatt.connected) {
      statusArea.textContent = `Disconnecting from ${
        bleDevice.name || bleDevice.id
      }...`;
      console.log(`Disconnecting from ${bleDevice.name || bleDevice.id}`);
      try {
        await bleDevice.gatt.disconnect();
        // The 'gattserverdisconnected' event should fire, calling onDisconnected()
      } catch (error) {
        statusArea.textContent = `Error disconnecting: ${error.message}`;
        console.error("Error disconnecting:", error);
        // Even on error, call onDisconnected to attempt cleanup
        onDisconnected({ target: bleDevice });
      }
    } else {
      console.log("Device already disconnected.");
      onDisconnected({ target: bleDevice }); // Ensure UI updates if already disconnected
    }
  }

  // Placeholder for functions to be developed in other subtasks
  // async function sendCommand(command) { ... }
  // function handleTelemetry(event) { ... }
  // function updateGraph(tempData) { ... }
  // function updateConfirmButtonVisibility(state) { ... }

  // Placeholder onclick handlers for new buttons (functionality in later subtasks)
  if (setTargetTempButton) {
    setTargetTempButton.onclick = async () => {
      const tempValue = parseInt(setTargetTempInput.value, 10);
      if (isNaN(tempValue) || tempValue < 0 || tempValue > 150) {
        statusArea.textContent =
          "Invalid Target Temperature. Must be a number between 0 and 150.";
        console.warn("Invalid target temperature input.");
        return;
      }
      await sendCommand(`TEMPERATURE ${tempValue}`, (response) => {
        if (response.error) {
          console.error("Error setting target temperature:", response.error);
          statusArea.textContent = `Error setting temp: ${response.error}`;
          appendToCommandOutput(
            `Error setting temp: ${response.error}`,
            "error"
          );
        } else {
          console.log("Set target temperature response:", response);
          statusArea.textContent = "Target temperature command sent.";
          appendToCommandOutput(
            `TEMPERATURE response: ${JSON.stringify(response)}`,
            "response"
          );
          // Update UI based on response if needed, e.g., if device confirms the set temp
          if (response.target_temperature !== undefined) {
            targetTempDisplay.textContent = `${response.target_temperature}°C`;
          }
        }
      });
    };
  }

  if (startProcessButton) {
    startProcessButton.onclick = async () => {
      console.log("Start Process button clicked.");
      await sendCommand("START", (response) => {
        if (response.error) {
          console.error("Error starting process:", response.error);
          statusArea.textContent = `Error starting: ${response.error}`;
          appendToCommandOutput(`Error starting: ${response.error}`, "error");
        } else {
          console.log("Start process response:", response);
          statusArea.textContent = "Start command sent.";
          appendToCommandOutput(
            `START response: ${JSON.stringify(response)}`,
            "response"
          );
          // Update UI based on response, e.g., state.started
          if (response.started !== undefined) {
            currentStateDisplay.textContent = response.started
              ? "Process Active"
              : "Idle";
          }
        }
      });
    };
  }

  if (stopProcessButton) {
    stopProcessButton.onclick = async () => {
      console.log("Stop Process button clicked.");
      await sendCommand("ABORT", (response) => {
        if (response.error) {
          console.error("Error stopping process:", response.error);
          statusArea.textContent = `Error stopping: ${response.error}`;
          appendToCommandOutput(`Error stopping: ${response.error}`, "error");
        } else {
          console.log("Stop process response:", response);
          statusArea.textContent = "Stop command sent.";
          appendToCommandOutput(
            `ABORT response: ${JSON.stringify(response)}`,
            "response"
          );
          if (response.started !== undefined && !response.started) {
            currentStateDisplay.textContent = "Idle";
          }
        }
      });
    };
  }

  if (getStatusButton) {
    getStatusButton.onclick = async () => {
      console.log("Get Device Status button clicked.");
      await sendCommand("SYNC", (response) => {
        if (response.error) {
          console.error("Error getting status:", response.error);
          statusArea.textContent = `Error getting status: ${response.error}`;
          appendToCommandOutput(
            `Error getting status: ${response.error}`,
            "error"
          );
        } else {
          console.log("Device status response:", response);
          statusArea.textContent = "Status received.";
          appendToCommandOutput(
            `SYNC response: ${JSON.stringify(response)}`,
            "response"
          );
          // Update UI elements with the response data
          if (response.temperature !== undefined) {
            currentTempDisplay.textContent = `${response.temperature}°C`;
          }
          if (response.target_temperature !== undefined) {
            targetTempDisplay.textContent = `${response.target_temperature}°C`;
          }
          if (response.state !== undefined) {
            currentStateDisplay.textContent =
              DEVICE_STATES[response.state] || `Unknown (${response.state})`;
            // Show/Hide modal based on state
            if (response.state === 2) {
              // WAIT_CONFIRM state
              // Assumes response.confirm_message is now sent by the device in SYNC
              showConfirmModal(
                response.confirm_message || "Please confirm the pending action."
              );
            } else {
              hideConfirmModal();
            }
          }
          // Potentially update temperature graph here too
          if (
            response.temperature !== undefined &&
            response.target_temperature !== undefined
          ) {
            updateTemperatureGraph(
              response.temperature,
              response.target_temperature,
              response.output
            );
          }

          // Update new SYNC data displays
          if (kpDisplay)
            kpDisplay.textContent =
              response.kp !== undefined ? response.kp : "N/A";
          if (kiDisplay)
            kiDisplay.textContent =
              response.ki !== undefined ? response.ki : "N/A";
          if (kdDisplay)
            kdDisplay.textContent =
              response.kd !== undefined ? response.kd : "N/A";
          if (hysteresisTempDisplay)
            hysteresisTempDisplay.textContent =
              response.hysteresis_degrees_c !== undefined
                ? response.hysteresis_degrees_c
                : "N/A";
          if (hysteresisTimeDisplay)
            hysteresisTimeDisplay.textContent =
              response.hysteresis_seconds !== undefined
                ? response.hysteresis_seconds
                : "N/A";
          if (volumeDisplay)
            volumeDisplay.textContent =
              response.volume_liters !== undefined
                ? response.volume_liters
                : "N/A";
          if (powerDisplay)
            powerDisplay.textContent =
              response.power_watts !== undefined ? response.power_watts : "N/A";
          if (tuningDisplay)
            tuningDisplay.textContent =
              response.tuning !== undefined
                ? response.tuning
                  ? "Active"
                  : "Inactive"
                : "N/A";
          if (confirmMessageDisplay)
            confirmMessageDisplay.textContent =
              response.confirm_message || "N/A";
          if (messageDisplay)
            messageDisplay.textContent = response.message || "N/A";
          if (messageL2Display)
            messageL2Display.textContent = response.message_line_2 || "N/A";
          if (messageL3Display)
            messageL3Display.textContent = response.message_line_3 || "N/A";

          // Populate PID config inputs
          if (prefPidKpInput && response.kp !== undefined)
            prefPidKpInput.value = response.kp;
          if (prefPidKiInput && response.ki !== undefined)
            prefPidKiInput.value = response.ki;
          if (prefPidKdInput && response.kd !== undefined)
            prefPidKdInput.value = response.kd;
          if (prefPidPonInput && response.pOn !== undefined)
            prefPidPonInput.value = response.pOn; // Assuming ESP32 sends pOn
          if (prefPidSampleTimeInput && response.time !== undefined) {
            prefPidSampleTimeInput.value = response.time;
            lastKnownSampleTime = response.time; // Store for sending PID command
          } else if (prefPidSampleTimeInput) {
            prefPidSampleTimeInput.value = lastKnownSampleTime; // Use default/last known
          }

          if (response.id && response.confirm_message) {
            lastMessageIdRequiringConfirmation = response.id;
            statusArea.textContent = `Action Required: ${
              response.message
            } (ID: 0x${response.id.toString(16)})`;
            confirmActionButton.style.display = "block";
            rejectActionButton.style.display = "block";
            console.log(
              `Confirmation required for message ID: 0x${response.id.toString(
                16
              )}`
            );
          } else {
            hideConfirmModal();
          }

          // Handle state-specific timer display for auto SYNC too
          // Note: `response` here is from the SYNC command in performSync
          // `clearStateTimer()` is called at the start of `startStateCountdown`
          if (
            response.state === 1 &&
            response.target_timer_time !== undefined
          ) {
            // Waiting Timer
            startStateCountdown(response.target_timer_time, "Timer Ends In");
          } else if (
            response.state === 3 &&
            response.target_preparing_time !== undefined
          ) {
            // PREPARING
            startStateCountdown(
              response.target_preparing_time,
              "Preparation Finishes In"
            );
          } else {
            clearStateTimer(); // If not in a countdown state, ensure timer is cleared.
          }
        }
      });
    };
  }

  if (confirmActionButton) {
    confirmActionButton.onclick = async () => {
      if (lastMessageIdRequiringConfirmation !== null) {
        if (
          await sendCommand(
            `CONFIRM ${lastMessageIdRequiringConfirmation}`,
            (response) => {
              if (response.error) {
                console.error("Error confirming action:", response.error);
                statusArea.textContent = `Error confirming: ${response.error}`;
                appendToCommandOutput(
                  `Error confirming action: ${response.error}`,
                  "error"
                );
              } else {
                console.log("Confirm action response:", response);
                statusArea.textContent = `Action ID 0x${lastMessageIdRequiringConfirmation.toString(
                  16
                )} confirmed.`;
                appendToCommandOutput(
                  `CONFIRM response: ${JSON.stringify(response)}`,
                  "response"
                );
              }
            }
          )
        ) {
          // statusArea.textContent is now handled in callback
        }
        confirmActionButton.style.display = "none";
        rejectActionButton.style.display = "none";
        lastMessageIdRequiringConfirmation = null;
      } else {
        statusArea.textContent = "No action pending confirmation.";
        console.warn("Confirm button clicked with no pending action.");
      }
    };
  }

  if (rejectActionButton) {
    rejectActionButton.onclick = async () => {
      if (lastMessageIdRequiringConfirmation !== null) {
        if (
          await sendCommand(
            `REJECT ${lastMessageIdRequiringConfirmation}`,
            (response) => {
              if (response.error) {
                console.error("Error rejecting action:", response.error);
                statusArea.textContent = `Error rejecting: ${response.error}`;
                appendToCommandOutput(
                  `Error rejecting action: ${response.error}`,
                  "error"
                );
              } else {
                console.log("Reject action response:", response);
                statusArea.textContent = `Action ID 0x${lastMessageIdRequiringConfirmation.toString(
                  16
                )} rejected.`;
                appendToCommandOutput(
                  `REJECT response: ${JSON.stringify(response)}`,
                  "response"
                );
              }
            }
          )
        ) {
          // statusArea.textContent is now handled in callback
        }
        confirmActionButton.style.display = "none";
        rejectActionButton.style.display = "none";
        lastMessageIdRequiringConfirmation = null;
      } else {
        statusArea.textContent = "No action pending rejection.";
        console.warn("Reject button clicked with no pending action.");
      }
    };
  }

  // Manual Command Sending
  if (sendManualCommandButton) {
    sendManualCommandButton.onclick = async () => {
      const commandStr = manualCommandInput.value.trim();
      if (!commandStr) {
        appendToCommandOutput("Command input is empty.", "warn");
        return;
      }
      appendToCommandOutput(`Sending manual command: ${commandStr}`, "info");
      await sendCommand(commandStr, (response) => {
        if (response.error) {
          console.error(
            `Manual command error for '${commandStr}':`,
            response.error
          );
          appendToCommandOutput(
            `Error for '${commandStr}': ${response.error}`,
            "error"
          );
        } else {
          console.log(`Manual command response for '${commandStr}':`, response);
          appendToCommandOutput(
            `Response for '${commandStr}': ${JSON.stringify(response)}`,
            "response"
          );
        }
      });
      manualCommandInput.value = ""; // Clear input after sending
    };
  }
  // Also allow sending with Enter key in the input field
  if (manualCommandInput) {
    manualCommandInput.addEventListener("keypress", (event) => {
      if (event.key === "Enter") {
        event.preventDefault(); // Prevent default form submission if it were in a form
        sendManualCommandButton.click(); // Trigger the button click
      }
    });
  }

  // --- Command Sending Function ---
  async function sendCommand(commandString, callback = null) {
    if (!commandCharacteristic) {
      statusArea.textContent = "Not connected to a device.";
      console.warn("sendCommand called while not connected.");
      return false;
    }

    const messageId = nextCommandId++;
    const fullCommand = `$${messageId} ${commandString}`;

    if (callback) {
      pendingCommands.set(messageId, callback);
      // Optional: Add a timeout for commands that don't get a response
      setTimeout(() => {
        if (pendingCommands.has(messageId)) {
          console.warn(`Command $${messageId} (${commandString}) timed out.`);
          pendingCommands.get(messageId)({ error: "Timeout" });
          pendingCommands.delete(messageId);
        }
      }, 5000); // 5-second timeout, adjust as needed
    }

    try {
      console.log(`Sending command: ${fullCommand}`);
      statusArea.textContent = `Sending: ${commandString}...`;
      const encoder = new TextEncoder();
      // Using writeValueWithResponse as we expect a JSON response with an ID
      await commandCharacteristic.writeValueWithResponse(
        encoder.encode(fullCommand + "\n") // Ensure newline if your device expects it
      );
      console.log(
        `Command $${messageId} (${commandString}) sent successfully.`
      );
      // The response will be handled by the characteristicvaluechanged event (handleTelemetry)
      return true; // Indicate command was sent
    } catch (error) {
      statusArea.textContent = `Error sending command ${commandString}: ${error.message}`;
      console.error(
        `Error sending command $${messageId} (${commandString}):`,
        error
      );
      if (pendingCommands.has(messageId)) {
        pendingCommands.get(messageId)({ error: error.message });
        pendingCommands.delete(messageId);
      }
      return false;
    }
  }

  // --- Handle Telemetry (Incoming Data) ---
  function handleTelemetry(event) {
    // Determine if event is from BLE (event.target.value) or WebSocket (event.data)
    let dataViewOrString;
    let isWebSocket = false;
    if (event.target && event.target.value) {
      dataViewOrString = event.target.value; // BLE
    } else if (event.data) {
      dataViewOrString = event.data; // WebSocket
      isWebSocket = true;
    } else {
      console.warn("Received telemetry event with no data.");
      return;
    }

    try {
      let jsonData;
      if (isWebSocket) {
        if (typeof dataViewOrString === "string") {
          jsonData = JSON.parse(dataViewOrString);
        } else {
          // Assuming it might be ArrayBuffer or Blob from WebSocket, needs decoding
          // This part might need adjustment based on how your WS sends data
          console.warn(
            "Received non-string data from WebSocket, decoding not implemented here."
          );
          return;
        }
      } else {
        // BLE: Assuming DataView, needs TextDecoder
        const decoder = new TextDecoder("utf-8");
        const receivedString = decoder.decode(dataViewOrString);
        jsonData = JSON.parse(receivedString);
      }

      appendToCommandOutput(
        `Received: ${JSON.stringify(jsonData)}`,
        "response"
      );

      // Handle command-specific responses based on 'id' if present
      if (jsonData.id && pendingCommands.has(jsonData.id)) {
        const callback = pendingCommands.get(jsonData.id);
        callback(jsonData); // Resolve the promise/execute callback
        pendingCommands.delete(jsonData.id); // Clean up
      }

      // Update UI based on jsonData.type
      if (jsonData.type) {
        switch (jsonData.type) {
          case "status_update": // Existing status update
            if (currentTempDisplay && jsonData.current_temp !== undefined) {
              currentTempDisplay.textContent =
                jsonData.current_temp.toFixed(2) + " °C";
            }
            if (targetTempDisplay && jsonData.target_temp !== undefined) {
              targetTempDisplay.textContent = `(Current: ${jsonData.target_temp.toFixed(
                2
              )} °C)`;
            }
            if (currentStateDisplay && jsonData.current_state !== undefined) {
              currentStateDisplay.textContent =
                DEVICE_STATES[jsonData.current_state] || "Unknown";
            }
            // ... any other fields from status_update ...
            break;
          case "preferences":
            if (preferencesValuesDiv) {
              // This part is for the OLD raw display, can be kept or removed if not needed.
              // We will now primarily populate the input fields.
              preferencesValuesDiv.innerHTML = ""; // Clear previous values
              const ul = document.createElement("ul");
              ul.style.listStyleType = "none";
              ul.style.paddingLeft = "0";
              for (const key in jsonData) {
                if (key !== "type" && key !== "status" && key !== "id") {
                  const li = document.createElement("li");
                  const strong = document.createElement("strong");
                  strong.textContent =
                    key
                      .replace(/_/g, " ")
                      .replace(/\b\w/g, (l) => l.toUpperCase()) + ": ";
                  li.appendChild(strong);
                  const span = document.createElement("span");
                  span.textContent = jsonData[key];
                  li.appendChild(span);
                  ul.appendChild(li);
                }
              }
              preferencesValuesDiv.appendChild(ul);
            }

            // Populate the new input fields
            if (prefSsidInput && jsonData.ssid !== undefined)
              prefSsidInput.value = jsonData.ssid;
            // Do not populate password field for security: prefPasswordInput.value = jsonData.password;
            if (prefVolumeInput && jsonData.volume_liters !== undefined)
              prefVolumeInput.value = jsonData.volume_liters;
            if (prefPowerInput && jsonData.power_watts !== undefined)
              prefPowerInput.value = jsonData.power_watts;
            if (
              prefHysteresisTempInput &&
              jsonData.hysteresis_degrees_c !== undefined
            )
              prefHysteresisTempInput.value = jsonData.hysteresis_degrees_c;
            if (
              prefHysteresisTimeInput &&
              jsonData.hysteresis_seconds !== undefined
            )
              prefHysteresisTimeInput.value = jsonData.hysteresis_seconds;

            if (prefPidKpInput && jsonData.kp !== undefined)
              prefPidKpInput.value = jsonData.kp;
            if (prefPidKiInput && jsonData.ki !== undefined)
              prefPidKiInput.value = jsonData.ki;
            if (prefPidKdInput && jsonData.kd !== undefined)
              prefPidKdInput.value = jsonData.kd;
            if (prefPidPonInput && jsonData.pOn !== undefined)
              prefPidPonInput.value = jsonData.pOn;
            if (prefPidSampleTimeInput && jsonData.time !== undefined)
              prefPidSampleTimeInput.value = jsonData.time;

            appendToCommandOutput(
              "Preferences loaded into editable fields.",
              "info"
            );
            break;
          case "list_files":
            console.log("Files listed:", jsonData.files);
            // You'll need to add UI elements to display these files
            // For example, populate a <ul> list
            const fileListElement = document.getElementById("fileList"); // Assuming you add this to HTML
            if (fileListElement) {
              fileListElement.innerHTML = ""; // Clear previous list
              jsonData.files.forEach((file) => {
                const listItem = document.createElement("li");
                listItem.textContent = `${file.name} (${file.size} bytes)`;
                li.style.padding = "2px 0";
                li.style.cursor = "pointer";
                li.setAttribute("data-filename", file.name);
                li.addEventListener("mouseover", () => {
                  li.style.textDecoration = "underline";
                });
                li.addEventListener("mouseout", () => {
                  li.style.textDecoration = "none";
                });

                li.addEventListener("click", async (event) => {
                  const filename = event.target.getAttribute("data-filename");
                  if (filename) {
                    appendToCommandOutput(
                      `Attempting to open file: ${filename}`,
                      "info"
                    );
                    // Highlight clicked item briefly (optional)
                    event.target.style.backgroundColor = "#e0e0e0";
                    setTimeout(() => {
                      event.target.style.backgroundColor = "";
                    }, 300);

                    await sendCommand(
                      `OPEN_FILE ${filename}`,
                      (openResponse) => {
                        if (openResponse.error) {
                          console.error(
                            `Error sending OPEN_FILE ${filename}:`,
                            openResponse.error
                          );
                          appendToCommandOutput(
                            `Error for OPEN_FILE ${filename}: ${openResponse.error}`,
                            "error"
                          );
                        } else {
                          // The OPEN_FILE command in C++ doesn't send specific JSON back on success via the command channel.
                          // It changes the device state to start reading from the file.
                          // Success here means the command was acknowledged by the BLE characteristic.
                          console.log(
                            `OPEN_FILE ${filename} command sent successfully. Response:`,
                            openResponse
                          );
                          appendToCommandOutput(
                            `OPEN_FILE ${filename} command sent. Device will attempt to open.`,
                            "response"
                          );
                          // Further confirmation of file opening would come from device behavior (e.g., logs or state changes).
                        }
                      }
                    );
                  }
                });
                ul.appendChild(li);
              });
              fileListOutput.appendChild(ul);
            }
            break;
          case "confirmation_required":
            // This logic seems to be for a different flow, adapt if needed
            // For instance, if the device sends a "CONFIRM_ACTION <id>" message
            // that your script needs to respond to with $id CONFIRM.
            // The current `confirmActionButton.onclick` handles sending a confirm.
            // This section might be for unsolicited requests for confirmation.
            lastMessageIdRequiringConfirmation = jsonData.messageId; // Assuming `data.messageId` comes from device
            statusArea.textContent = `Action Required: ${
              jsonData.message
            } (ID: 0x${jsonData.messageId.toString(16)})`;
            confirmActionButton.style.display = "block";
            rejectActionButton.style.display = "block";
            console.log(
              `Confirmation required for message ID: 0x${jsonData.messageId.toString(
                16
              )}`
            );
            break;
          case "error_message":
            statusArea.textContent = `Device Error: ${jsonData.message}`;
            console.error(
              `Device Error: ${jsonData.message} (Code: ${jsonData.errorCode})`
            );
            break;
          case "utc":
            // Handling GET_TIME response
            const date = new Date(jsonData.utc * 1000); // Convert seconds to milliseconds
            console.log("Device time (UTC):", date.toUTCString());
            statusArea.textContent = `Device Time: ${date.toLocaleString()}`;
            // You could display this time in a dedicated UI element
            break;
          default:
            console.log("Received unhandled telemetry structure:", jsonData);
        }
      } else {
        console.log("Received unhandled telemetry structure:", jsonData);
      }
    } catch (error) {
      console.error("Error parsing telemetry JSON or handling data:", error);
      // statusArea.textContent = `Error processing telemetry: ${error.message}`;
      // It might be binary data or non-JSON, handle accordingly
      // For now, we'll just log, as raw string is already displayed.
    }
  }

  // --- Function to update the temperature graph ---
  function updateTemperatureGraph(
    newCurrentTemp,
    newTargetTemp,
    newOutputValue
  ) {
    if (!temperatureChart) return;

    const now = new Date();
    // Using a more concise time format for labels, including seconds for real-time view
    const label = `${String(now.getHours()).padStart(2, "0")}:${String(
      now.getMinutes()
    ).padStart(2, "0")}:${String(now.getSeconds()).padStart(2, "0")}`;

    temperatureChart.data.labels.push(label);
    temperatureChart.data.datasets[0].data.push(newCurrentTemp);

    if (newTargetTemp !== undefined && temperatureChart.data.datasets[1]) {
      temperatureChart.data.datasets[1].data.push(newTargetTemp);
    }

    // Add Output value to the third dataset
    if (newOutputValue !== undefined && temperatureChart.data.datasets[2]) {
      temperatureChart.data.datasets[2].data.push(newOutputValue);
    }

    // Limit the number of data points shown for performance
    while (temperatureChart.data.labels.length > configurableMaxDataPoints) {
      temperatureChart.data.labels.shift();
      temperatureChart.data.datasets[0].data.shift();
      if (
        temperatureChart.data.datasets[1] &&
        temperatureChart.data.datasets[1].data.length > 0
      ) {
        temperatureChart.data.datasets[1].data.shift();
      }
      if (
        temperatureChart.data.datasets[2] &&
        temperatureChart.data.datasets[2].data.length > 0
      ) {
        temperatureChart.data.datasets[2].data.shift();
      }
    }
    temperatureChart.update();
  }

  // Initialize Chart.js
  if (tempCanvas) {
    const ctx = tempCanvas.getContext("2d");
    temperatureChart = new Chart(ctx, {
      type: "line",
      data: {
        labels: [], // For timestamps or sequence numbers
        datasets: [
          {
            label: "Current Temperature",
            data: [], // Temperature values
            borderColor: "rgb(75, 192, 192)",
            backgroundColor: "rgba(75, 192, 192, 0.2)",
            tension: 0.1,
            fill: true,
            yAxisID: "y", // Assign to the first y-axis
          },
          {
            label: "Target Temperature",
            data: [],
            borderColor: "rgb(255, 99, 132)",
            backgroundColor: "rgba(255, 99, 132, 0.2)",
            tension: 0.1,
            fill: false,
            pointRadius: 2,
            borderDash: [5, 5],
            yAxisID: "y", // Assign to the first y-axis
          },
          {
            label: "Output (%)", // New dataset for PID Output
            data: [],
            borderColor: "rgb(54, 162, 235)", // Blue
            backgroundColor: "rgba(54, 162, 235, 0.2)",
            tension: 0.1,
            fill: false,
            yAxisID: "y1", // Assign to the new y-axis
            pointRadius: 2,
          },
        ],
      },
      options: {
        responsive: true,
        maintainAspectRatio: false,
        scales: {
          x: {
            type: "category",
            ticks: {
              autoSkip: true,
              maxTicksLimit: 10,
            },
          },
          y: {
            // Primary Y-axis (Temperature)
            type: "linear",
            display: true,
            position: "left",
            beginAtZero: false,
            title: {
              display: true,
              text: "Temperature (°C)",
            },
          },
          y1: {
            // Secondary Y-axis (Output %)
            type: "linear",
            display: true,
            position: "right",
            min: 0,
            max: 100,
            title: {
              display: true,
              text: "Output (%)",
            },
            // Ensure grid lines from this axis don't clash too much
            grid: {
              drawOnChartArea: false, // Only draw grid lines for the first Y axis or customize
            },
          },
        },
        animation: false,
        plugins: {
          legend: {
            position: "top",
          },
        },
      },
    });
  }

  // Auto-SYNC Logic
  function performSync() {
    if (!bleDevice || !bleDevice.gatt.connected) {
      console.warn("Auto-SYNC: Device not connected. Stopping auto-sync.");
      stopAutoSync();
      if (autoSyncToggle) autoSyncToggle.checked = false;
      isAutoSyncEnabled = false;
      return;
    }
    console.log("Auto-SYNC: Performing SYNC...");
    // Re-use the getStatusButton's click logic for the SYNC command and UI updates
    if (getStatusButton) {
      // We don't want to trigger getStatusButton.onclick directly as it has its own console logs for manual clicks.
      // Instead, directly call sendCommand with the SYNC logic.
      sendCommand("SYNC", (response) => {
        if (response.error) {
          console.error("Auto-SYNC Error:", response.error);
          appendToCommandOutput(`Auto-SYNC Error: ${response.error}`, "error");
        } else {
          console.log("Auto-SYNC successful:", response);
          appendToCommandOutput(
            `Auto-SYNC: ${JSON.stringify(response)}`,
            "telemetry"
          ); // Log as telemetry
          // Update UI elements (already handled by this callback structure if it mirrors getStatusButton)
          if (response.temperature !== undefined) {
            currentTempDisplay.textContent = `${response.temperature}°C`;
          }
          if (response.target_temperature !== undefined) {
            targetTempDisplay.textContent = `${response.target_temperature}°C`;
          }
          if (response.state !== undefined) {
            currentStateDisplay.textContent =
              DEVICE_STATES[response.state] || `Unknown (${response.state})`;
            // Show/Hide modal based on state
            if (response.state === 2) {
              // WAIT_CONFIRM state
              // Assumes response.confirm_message is now sent by the device in SYNC
              showConfirmModal(
                response.confirm_message || "Please confirm the pending action."
              );
            } else {
              hideConfirmModal();
            }
          }
          if (
            response.temperature !== undefined &&
            response.target_temperature !== undefined
          ) {
            updateTemperatureGraph(
              response.temperature,
              response.target_temperature,
              response.output
            );
          }
        }
      });
    }
  }

  function startAutoSync() {
    if (autoSyncTimerId) {
      clearInterval(autoSyncTimerId); // Clear any existing timer
    }
    if (bleDevice && bleDevice.gatt.connected && isAutoSyncEnabled) {
      performSync(); // Perform an initial sync immediately
      autoSyncTimerId = setInterval(performSync, autoSyncIntervalMs);
      console.log(
        `Auto-SYNC started with interval: ${autoSyncIntervalMs / 1000}s`
      );
      appendToCommandOutput(
        `Auto-SYNC started. Interval: ${autoSyncIntervalMs / 1000}s.`,
        "info"
      );
    } else {
      console.log(
        "Auto-SYNC not started (device not connected or not enabled)."
      );
    }
  }

  function stopAutoSync() {
    if (autoSyncTimerId) {
      clearInterval(autoSyncTimerId);
      autoSyncTimerId = null;
      console.log("Auto-SYNC stopped.");
      appendToCommandOutput("Auto-SYNC stopped.", "info");
    }
  }

  if (autoSyncToggle) {
    isAutoSyncEnabled = autoSyncToggle.checked;
    autoSyncToggle.addEventListener("change", () => {
      isAutoSyncEnabled = autoSyncToggle.checked;
      if (isAutoSyncEnabled) {
        if (syncIntervalInput) {
          // Ensure input exists before reading
          autoSyncIntervalMs =
            parseInt(syncIntervalInput.value, 10) * 1000 || 5000;
        }
        startAutoSync();
      } else {
        stopAutoSync();
      }
    });
  }

  if (syncIntervalInput) {
    autoSyncIntervalMs = parseInt(syncIntervalInput.value, 10) * 1000 || 5000;
    syncIntervalInput.addEventListener("change", () => {
      const newIntervalSeconds = parseInt(syncIntervalInput.value, 10);
      if (
        !isNaN(newIntervalSeconds) &&
        newIntervalSeconds >= 1 &&
        newIntervalSeconds <= 300
      ) {
        autoSyncIntervalMs = newIntervalSeconds * 1000;
        console.log(
          `Auto-SYNC interval updated to: ${autoSyncIntervalMs / 1000}s`
        );
        appendToCommandOutput(
          `Auto-SYNC interval set to ${autoSyncIntervalMs / 1000}s.`,
          "info"
        );
        if (isAutoSyncEnabled) {
          startAutoSync(); // Restart with new interval if already enabled
        }
      } else {
        syncIntervalInput.value = autoSyncIntervalMs / 1000; // Revert to old value if invalid
        console.warn("Invalid Auto-SYNC interval. Reverted.");
      }
    });
  }

  // List Files Button
  if (listFilesButton) {
    listFilesButton.onclick = async () => {
      if (!bleDevice || !bleDevice.gatt.connected) {
        appendToCommandOutput(
          "Cannot list files: Device not connected.",
          "error"
        );
        if (fileListOutput)
          fileListOutput.textContent = "Device not connected.";
        return;
      }
      appendToCommandOutput("Sending LIST_FILES command...", "info");
      if (fileListOutput) fileListOutput.textContent = "Listing files...";

      await sendCommand("LIST_FILES", (response) => {
        if (response.error) {
          console.error("Error listing files:", response.error);
          if (fileListOutput)
            fileListOutput.textContent = `Error: ${response.error}`;
          // The error will also be logged to commandOutputArea by sendCommand's callback general handling
        } else if (
          response.type === "list_files" &&
          Array.isArray(response.files)
        ) {
          console.log("Files listed:", response.files);
          if (fileListOutput) {
            fileListOutput.innerHTML = ""; // Clear previous list
            if (response.files.length === 0) {
              fileListOutput.textContent = "No files found on device.";
            } else {
              const ul = document.createElement("ul");
              ul.style.listStyleType = "none";
              ul.style.paddingLeft = "0";
              response.files.forEach((file) => {
                const li = document.createElement("li");
                li.textContent = `${file.name} (${file.size} bytes)`;
                li.style.padding = "2px 0";
                li.style.cursor = "pointer";
                li.setAttribute("data-filename", file.name);
                li.addEventListener("mouseover", () => {
                  li.style.textDecoration = "underline";
                });
                li.addEventListener("mouseout", () => {
                  li.style.textDecoration = "none";
                });

                li.addEventListener("click", async (event) => {
                  const filename = event.target.getAttribute("data-filename");
                  if (filename) {
                    appendToCommandOutput(
                      `Attempting to open file: ${filename}`,
                      "info"
                    );
                    // Highlight clicked item briefly (optional)
                    event.target.style.backgroundColor = "#e0e0e0";
                    setTimeout(() => {
                      event.target.style.backgroundColor = "";
                    }, 300);

                    await sendCommand(
                      `OPEN_FILE ${filename}`,
                      (openResponse) => {
                        if (openResponse.error) {
                          console.error(
                            `Error sending OPEN_FILE ${filename}:`,
                            openResponse.error
                          );
                          appendToCommandOutput(
                            `Error for OPEN_FILE ${filename}: ${openResponse.error}`,
                            "error"
                          );
                        } else {
                          // The OPEN_FILE command in C++ doesn't send specific JSON back on success via the command channel.
                          // It changes the device state to start reading from the file.
                          // Success here means the command was acknowledged by the BLE characteristic.
                          console.log(
                            `OPEN_FILE ${filename} command sent successfully. Response:`,
                            openResponse
                          );
                          appendToCommandOutput(
                            `OPEN_FILE ${filename} command sent. Device will attempt to open.`,
                            "response"
                          );
                          // Further confirmation of file opening would come from device behavior (e.g., logs or state changes).
                        }
                      }
                    );
                  }
                });
                ul.appendChild(li);
              });
              fileListOutput.appendChild(ul);
            }
          }
          // The full response is already logged to commandOutputArea by sendCommand's callback
        } else {
          console.warn(
            "Received unexpected response for LIST_FILES:",
            response
          );
          if (fileListOutput)
            fileListOutput.textContent = "Unexpected response from device.";
          // Log to commandOutputArea is handled by sendCommand callback
        }
      });
    };
  }

  // Clear Command Output Button
  if (clearCommandOutputButton) {
    clearCommandOutputButton.onclick = () => {
      if (commandOutputArea) {
        commandOutputArea.innerHTML =
          "<div style='color: #6c757d;'>[Log cleared]</div>"; // Clear and add a placeholder
      }
    };
  }

  // Modal Helper Functions
  function showConfirmModal(message) {
    if (modalConfirmMessage) modalConfirmMessage.textContent = message;
    if (confirmationModal) confirmationModal.style.display = "flex"; // Use flex to help with centering
    // Adjust modal-content style directly for centering if needed, or use CSS classes
    const modalContent = confirmationModal
      ? confirmationModal.querySelector(".modal-content")
      : null;
    if (modalContent) {
      // Basic centering, can be enhanced with CSS
      modalContent.style.display = "block";
      modalContent.style.position = "absolute";
      modalContent.style.left = "50%";
      modalContent.style.top = "50%";
      modalContent.style.transform = "translate(-50%, -50%)";
    }
  }

  function hideConfirmModal() {
    if (confirmationModal) confirmationModal.style.display = "none";
  }

  // Modal Button Listeners
  if (modalConfirmBtn) {
    modalConfirmBtn.onclick = async () => {
      appendToCommandOutput("Modal: Confirm button clicked.", "info");
      // The device expects a "CONFIRM" command. The `ENTER_CONFIRM` command sets a message,
      // and the state machine transitions. A simple "CONFIRM" might not be enough if it expects an ID.
      // For now, let's assume the device simply needs to be taken out of confirmState by a specific command
      // or by the next logical command if it's a step in a sequence.
      // If the `confirmState` in C++ is just a pause, perhaps sending the *next* command implicitly confirms.
      // Or, it might expect a simple "CONFIRM" string, or the original `state.confirm_message` to be echoed back.
      // Let's try sending a generic "CONFIRM" for now. This might need adjustment based on device firmware.
      // If the old confirmActionButton flow with message IDs is still relevant, this needs care.
      // This modal is tied to state 2, not necessarily to `lastMessageIdRequiringConfirmation`.

      await sendCommand("CONFIRM", (response) => {
        // Placeholder: Device might need a different command
        if (response.error) {
          appendToCommandOutput(
            `Modal Confirm Error: ${response.error}`,
            "error"
          );
        } else {
          appendToCommandOutput("Modal Confirm command sent.", "response");
        }
      });
      hideConfirmModal();
      // After confirming, we might want to trigger a SYNC to get the new state
      if (isAutoSyncEnabled) performSync();
      else if (getStatusButton) getStatusButton.onclick();
    };
  }

  if (modalRejectBtn) {
    modalRejectBtn.onclick = async () => {
      appendToCommandOutput("Modal: Reject button clicked.", "info");
      // Similar to confirm, the device might expect "REJECT", "CANCEL", or "ABORT".
      // "ABORT" is an existing command that resets the state machine.
      await sendCommand("ABORT", (response) => {
        if (response.error) {
          appendToCommandOutput(
            `Modal Reject (ABORT) Error: ${response.error}`,
            "error"
          );
        } else {
          appendToCommandOutput(
            "Modal Reject (ABORT) command sent.",
            "response"
          );
        }
      });
      hideConfirmModal();
      // After rejecting, trigger a SYNC to get the new state (likely Idle after ABORT)
      if (isAutoSyncEnabled) performSync();
      else if (getStatusButton) getStatusButton.onclick();
    };
  }

  // --- State Timer Functions ---
  function clearStateTimer() {
    if (stateCountdownIntervalId) {
      clearInterval(stateCountdownIntervalId);
      stateCountdownIntervalId = null;
    }
    if (stateTimerDisplayContainer)
      stateTimerDisplayContainer.style.display = "none";
    if (stateTimerLabel) stateTimerLabel.textContent = "";
    if (stateTimerValue) stateTimerValue.textContent = "00:00";
  }

  function updateStateTimerDisplay(targetTimestampSeconds, labelText) {
    if (!stateTimerDisplayContainer || !stateTimerLabel || !stateTimerValue)
      return false;

    const nowSeconds = Math.floor(Date.now() / 1000);
    let remainingSeconds = targetTimestampSeconds - nowSeconds;

    if (remainingSeconds < 0) remainingSeconds = 0;

    const minutes = Math.floor(remainingSeconds / 60);
    const seconds = remainingSeconds % 60;

    stateTimerLabel.textContent = labelText;
    stateTimerValue.textContent = `${String(minutes).padStart(2, "0")}:${String(
      seconds
    ).padStart(2, "0")}`;
    stateTimerDisplayContainer.style.display = "block";

    return remainingSeconds > 0;
  }

  function startStateCountdown(targetTimestampSeconds, labelText) {
    clearStateTimer(); // Clear any existing timer first
    if (updateStateTimerDisplay(targetTimestampSeconds, labelText)) {
      stateCountdownIntervalId = setInterval(() => {
        if (!updateStateTimerDisplay(targetTimestampSeconds, labelText)) {
          clearStateTimer(); // Stop interval if time runs out
          // Optionally, trigger a SYNC here to get the new state from device
          if (isAutoSyncEnabled) performSync();
          else if (getStatusButton) getStatusButton.onclick();
        }
      }, 1000);
    }
  }

  // Add event listener for the Get Preferences button
  if (getPreferencesButton) {
    getPreferencesButton.onclick = async () => {
      if (!commandCharacteristic && !ws) {
        // Check if connected (BLE or WS)
        appendToCommandOutput("Not connected to device.", "error");
        return;
      }
      appendToCommandOutput(
        "Sending PREFERENCES command (via button)...",
        "info"
      );
      // Use existing sendCommand if it's suitable for commands expecting JSON response
      // or implement a direct send if needed. Assuming sendCommand handles it:
      sendCommand("PREFERENCES");
    };
  }

  // Add event listener for the preferences <details> toggle
  if (preferencesSectionDetails) {
    preferencesSectionDetails.addEventListener("toggle", async (event) => {
      if (preferencesSectionDetails.open) {
        // Details section was opened
        if (!commandCharacteristic && !ws) {
          appendToCommandOutput(
            "Not connected. Cannot fetch preferences for details section.",
            "error"
          );
          return;
        }
        appendToCommandOutput(
          "Preferences section opened. Sending PREFERENCES command...",
          "info"
        );
        sendCommand("PREFERENCES");
      }
    });
  }

  // Add event listener for Apply Preferences button
  if (applyPreferencesButton) {
    applyPreferencesButton.onclick = async () => {
      if (!commandCharacteristic && !ws) {
        appendToCommandOutput("Not connected to device.", "error");
        return;
      }
      appendToCommandOutput("Applying preferences...", "info");

      let commandsSent = 0;
      let commandsSuccessfullyAcknowledged = 0;

      // Helper to send and track commands
      const sendPreferenceCommand = async (cmd) => {
        commandsSent++;
        const success = await sendCommand(cmd, (response) => {
          if (response && !response.error) {
            commandsSuccessfullyAcknowledged++;
            appendToCommandOutput(`Command \"${cmd}\" ACK.`, "response");
          } else {
            appendToCommandOutput(
              `Command \"${cmd}\" failed or no ACK: ${
                response ? response.error : "Unknown"
              }.`,
              "error"
            );
          }
        });
        if (!success) {
          // sendCommand itself failed (e.g. BLE write error)
          appendToCommandOutput(`Failed to send command \"${cmd}\".`, "error");
        }
      };

      // SSID (only if not empty)
      if (prefSsidInput && prefSsidInput.value.trim() !== "") {
        await sendPreferenceCommand(`SSID ${prefSsidInput.value.trim()}`);
      }
      // Password (only if not empty - implies user wants to change it)
      if (prefPasswordInput && prefPasswordInput.value !== "") {
        // Don't trim password, spaces can be valid
        await sendPreferenceCommand(`PASSWORD ${prefPasswordInput.value}`);
        prefPasswordInput.value = ""; // Clear after sending for security
      }
      // Volume
      if (prefVolumeInput && prefVolumeInput.value !== "") {
        await sendPreferenceCommand(
          `VOLUME ${parseFloat(prefVolumeInput.value)}`
        );
      }
      // Power
      if (prefPowerInput && prefPowerInput.value !== "") {
        await sendPreferenceCommand(
          `POWER ${parseFloat(prefPowerInput.value)}`
        );
      }
      // Hysteresis Temp
      if (prefHysteresisTempInput && prefHysteresisTempInput.value !== "") {
        await sendPreferenceCommand(
          `HYSTERESIS_TEMP ${parseFloat(prefHysteresisTempInput.value)}`
        );
      }
      // Hysteresis Time
      if (prefHysteresisTimeInput && prefHysteresisTimeInput.value !== "") {
        await sendPreferenceCommand(
          `HYSTERESIS_TIME ${parseInt(prefHysteresisTimeInput.value, 10)}`
        );
      }

      // PID settings (sent as one command)
      if (
        prefPidKpInput &&
        prefPidKiInput &&
        prefPidKdInput &&
        prefPidPonInput &&
        prefPidSampleTimeInput
      ) {
        const kp = parseFloat(prefPidKpInput.value);
        const ki = parseFloat(prefPidKiInput.value);
        const kd = parseFloat(prefPidKdInput.value);
        const pOn = parseInt(prefPidPonInput.value, 10); // Ensure it's int 0 or 1
        const sampleTime = parseInt(prefPidSampleTimeInput.value, 10);

        if (![kp, ki, kd, pOn, sampleTime].some(isNaN)) {
          // Check if all are valid numbers
          if (pOn === 0 || pOn === 1) {
            await sendPreferenceCommand(
              `PID ${kp} ${ki} ${kd} ${pOn} ${sampleTime}`
            );
          } else {
            appendToCommandOutput(
              "Invalid PID P On value (must be 0 or 1). PID settings not sent.",
              "error"
            );
          }
        } else {
          appendToCommandOutput(
            "Invalid PID numeric values. PID settings not sent.",
            "error"
          );
        }
      }

      // After sending all individual preference commands, send SAVE
      // Wait a brief moment to allow previous commands to be processed by ESP32 if sent rapidly
      setTimeout(async () => {
        appendToCommandOutput(
          "Sending SAVE command to persist settings...",
          "info"
        );
        await sendPreferenceCommand("SAVE");

        // Optionally, refresh preferences display after saving
        setTimeout(() => {
          if (getPreferencesButton) getPreferencesButton.click();
          appendToCommandOutput(
            `Preference update attempt finished. Sent ${commandsSent} commands, ${commandsSuccessfullyAcknowledged} acknowledged.`,
            "info"
          );
        }, 1000); // Wait a bit more before refreshing
      }, 500); // Adjust delay as needed
    };
  }
});

// PWA Service Worker Registration
if ("serviceWorker" in navigator) {
  window.addEventListener("load", () => {
    navigator.serviceWorker
      .register("/sw.js")
      .then((registration) => {
        console.log(
          "ServiceWorker: Registration successful, scope is:",
          registration.scope
        );
      })
      .catch((error) => {
        console.log("ServiceWorker: Registration failed:", error);
      });
  });
}
