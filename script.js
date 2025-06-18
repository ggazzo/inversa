document.addEventListener("DOMContentLoaded", () => {
  const connectButton = document.getElementById("connectButton");
  const statusArea = document.getElementById("statusArea");
  const currentTemperatureDisplay = document.getElementById("currentTemp");
  const targetTemperatureDisplay = document.getElementById("targetTempDisplay");
  const currentStateDisplay = document.getElementById("currentState");
  const confirmActionButton = document.getElementById("confirmActionButton");
  const rejectActionButton = document.getElementById("rejectActionButton");

  // New UI elements for SYNC data
  const proportionalGainDisplay = document.getElementById("kp");
  const integralGainDisplay = document.getElementById("ki");
  const derivativeGainDisplay = document.getElementById("kd");
  const hysteresisTemperatureDisplay =
    document.getElementById("hysteresisTemp");
  const hysteresisTimeDisplay = document.getElementById("hysteresisTime");
  const volumeDisplay = document.getElementById("volume");
  const powerDisplay = document.getElementById("power");
  const tuningDisplay = document.getElementById("tuning");
  const confirmMessageDisplay = document.getElementById("confirmMessage");
  const messageDisplay = document.getElementById("message");
  const messageLine2Display = document.getElementById("messageL2");
  const messageLine3Display = document.getElementById("messageL3");

  // New UI elements for Subtask 15.2
  const setTargetTemperatureInput =
    document.getElementById("setTargetTempInput");
  const setTargetTemperatureButton = document.getElementById(
    "setTargetTempButton"
  );
  const startProcessButton = document.getElementById("startProcessButton");
  const stopProcessButton = document.getElementById("stopProcessButton");
  const getStatusButton = document.getElementById("getStatusButton");
  const setTimeButton = document.getElementById("set_time_button");

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
  const preferenceSsidInput = document.getElementById("prefSsidInput");
  const preferencePasswordInput = document.getElementById("prefPasswordInput");
  const preferenceVolumeInput = document.getElementById("prefVolumeInput");
  const preferencePowerInput = document.getElementById("prefPowerInput");
  const preferenceHysteresisTemperatureInput = document.getElementById(
    "prefHysteresisTempInput"
  );
  const preferenceHysteresisTimeInput = document.getElementById(
    "prefHysteresisTimeInput"
  );
  const preferencePidProportionalGainInput =
    document.getElementById("prefPidKpInput");
  const preferencePidIntegralGainInput =
    document.getElementById("prefPidKiInput");
  const preferencePidDerivativeGainInput =
    document.getElementById("prefPidKdInput");
  const preferencePidProportionalOnInput =
    document.getElementById("prefPidPonInput");
  const preferencePidSampleTimeInput = document.getElementById(
    "prefPidSampleTimeInput"
  );

  // The <details> element for preferences
  const preferencesSectionDetails =
    document.getElementById("preferencesSection");

  let autoSyncIntervalMs = 5000; // Default 5 seconds
  let isAutoSyncEnabled = true; // Changed to true by default
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

  // Tuning Command Variables
  let tuningInterval = null;
  let currentTuningStep = 0;
  let totalTuningSteps = 0;
  let tuningTargetTemp = 0;

  // Tuning Command Functions
  function startTuning() {
    const targetTemp = parseFloat(
      document.getElementById("tuningTempInput").value
    );
    const steps = parseInt(document.getElementById("tuningStepsInput").value);

    if (isNaN(targetTemp) || isNaN(steps)) {
      alert("Please enter valid values for all tuning parameters");
      return;
    }

    if (steps < 1 || steps > 100) {
      alert("Number of steps must be between 1 and 100");
      return;
    }

    currentTuningStep = 0;
    totalTuningSteps = steps;
    tuningTargetTemp = targetTemp;

    // Update UI
    document.getElementById("startTuningButton").style.display = "none";
    document.getElementById("stopTuningButton").style.display = "inline-block";
    document.getElementById("tuningStatus").style.display = "block";
    document.getElementById("totalSteps").textContent = steps;
    document.getElementById("tuningTargetTemp").textContent =
      targetTemp.toFixed(1);

    // Start tuning process
    executeTuningStep();
  }

  function stopTuning() {
    if (tuningInterval) {
      clearInterval(tuningInterval);
      tuningInterval = null;
    }

    // Reset UI
    document.getElementById("startTuningButton").style.display = "inline-block";
    document.getElementById("stopTuningButton").style.display = "none";
    document.getElementById("tuningStatus").style.display = "none";

    // Send stop command to device
    sendCommand("STOP_TUNING");
  }

  function executeTuningStep() {
    if (currentTuningStep >= totalTuningSteps) {
      stopTuning();
      return;
    }

    // Calculate temperature for this step
    const stepTemp = tuningTargetTemp;

    // Send command to device
    sendCommand(
      `TUNING_STEP:${stepTemp.toFixed(1)}:${
        currentTuningStep + 1
      }:${totalTuningSteps}`
    );

    // Update UI
    document.getElementById("currentStep").textContent = currentTuningStep + 1;

    currentTuningStep++;
  }

  // Add event listeners for tuning controls
  document
    .getElementById("startTuningButton")
    .addEventListener("click", startTuning);
  document
    .getElementById("stopTuningButton")
    .addEventListener("click", stopTuning);

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
        currentTemperatureDisplay.textContent = "N/A";
        targetTemperatureDisplay.textContent = "N/A";
        currentStateDisplay.textContent = "N/A";

        // Reset new SYNC data displays
        if (proportionalGainDisplay)
          proportionalGainDisplay.textContent = "N/A";
        if (integralGainDisplay) integralGainDisplay.textContent = "N/A";
        if (derivativeGainDisplay) derivativeGainDisplay.textContent = "N/A";
        if (hysteresisTemperatureDisplay)
          hysteresisTemperatureDisplay.textContent = "N/A";
        if (hysteresisTimeDisplay) hysteresisTimeDisplay.textContent = "N/A";
        if (volumeDisplay) volumeDisplay.textContent = "N/A";
        if (powerDisplay) powerDisplay.textContent = "N/A";
        if (tuningDisplay) tuningDisplay.textContent = "N/A";
        if (confirmMessageDisplay) confirmMessageDisplay.textContent = "N/A";
        if (messageDisplay) messageDisplay.textContent = "N/A";
        if (messageLine2Display) messageLine2Display.textContent = "N/A";
        if (messageLine3Display) messageLine3Display.textContent = "N/A";

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
    currentTemperatureDisplay.textContent = "N/A";
    targetTemperatureDisplay.textContent = "N/A";
    currentStateDisplay.textContent = "N/A";

    // Reset new SYNC data displays on disconnect
    if (proportionalGainDisplay) proportionalGainDisplay.textContent = "N/A";
    if (integralGainDisplay) integralGainDisplay.textContent = "N/A";
    if (derivativeGainDisplay) derivativeGainDisplay.textContent = "N/A";
    if (hysteresisTemperatureDisplay)
      hysteresisTemperatureDisplay.textContent = "N/A";
    if (hysteresisTimeDisplay) hysteresisTimeDisplay.textContent = "N/A";
    if (volumeDisplay) volumeDisplay.textContent = "N/A";
    if (powerDisplay) powerDisplay.textContent = "N/A";
    if (tuningDisplay) tuningDisplay.textContent = "N/A";
    if (confirmMessageDisplay) confirmMessageDisplay.textContent = "N/A";
    if (messageDisplay) messageDisplay.textContent = "N/A";
    if (messageLine2Display) messageLine2Display.textContent = "N/A";
    if (messageLine3Display) messageLine3Display.textContent = "N/A";

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
  if (setTargetTemperatureButton) {
    setTargetTemperatureButton.onclick = async () => {
      const tempValue = parseInt(setTargetTemperatureInput.value, 10);
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
            targetTemperatureDisplay.textContent = `${response.target_temperature}°C`;
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

  if (setTimeButton) {
    setTimeButton.onclick = async () => {
      console.log("Set Time button clicked.");

      // 2025-06-03T12:00:00
      const event = new Date();
      const isoTime = event.toISOString().split(".")[0];

      await sendCommand(`SET_DATE ${isoTime}`, (response) => {
        console.log("Set Time response:", response);
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
          if (response.current_temperature !== undefined) {
            currentTemperatureDisplay.textContent = `${response.current_temperature}°C`;
          }
          if (response.target_temperature !== undefined) {
            targetTemperatureDisplay.textContent = `${response.target_temperature}°C`;
          }
          if (response.device_state !== undefined) {
            currentStateDisplay.textContent =
              DEVICE_STATES[response.device_state] ||
              `Unknown (${response.device_state})`;
            // Show/Hide modal based on state
            if (response.device_state === 2) {
              // WAIT_CONFIRM state
              showConfirmModal(
                response.confirmation_message ||
                  "Please confirm the pending action."
              );
            } else {
              hideConfirmModal();
            }
          }
          if (
            response.current_temperature !== undefined &&
            response.target_temperature !== undefined
          ) {
            updateTemperatureGraph(
              response.current_temperature,
              response.target_temperature,
              response.pid_output
            );
          }

          // Update new SYNC data displays
          if (proportionalGainDisplay)
            proportionalGainDisplay.textContent =
              response.proportional_gain !== undefined
                ? response.proportional_gain
                : "N/A";
          if (integralGainDisplay)
            integralGainDisplay.textContent =
              response.integral_gain !== undefined
                ? response.integral_gain
                : "N/A";
          if (derivativeGainDisplay)
            derivativeGainDisplay.textContent =
              response.derivative_gain !== undefined
                ? response.derivative_gain
                : "N/A";
          if (hysteresisTemperatureDisplay)
            hysteresisTemperatureDisplay.textContent =
              response.hysteresis_temperature !== undefined
                ? response.hysteresis_temperature
                : "N/A";
          if (hysteresisTimeDisplay)
            hysteresisTimeDisplay.textContent =
              response.hysteresis_time !== undefined
                ? response.hysteresis_time
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
              response.is_tuning_active !== undefined
                ? response.is_tuning_active
                  ? "Active"
                  : "Inactive"
                : "N/A";
          if (confirmMessageDisplay)
            confirmMessageDisplay.textContent =
              response.confirmation_message || "N/A";
          if (messageDisplay)
            messageDisplay.textContent = response.status_message || "N/A";
          if (messageLine2Display)
            messageLine2Display.textContent =
              response.status_message_line_2 || "N/A";
          if (messageLine3Display)
            messageLine3Display.textContent =
              response.status_message_line_3 || "N/A";

          // Populate PID config inputs
          if (
            preferencePidProportionalGainInput &&
            response.proportional_gain !== undefined
          )
            preferencePidProportionalGainInput.value =
              response.proportional_gain;
          if (
            preferencePidIntegralGainInput &&
            response.integral_gain !== undefined
          )
            preferencePidIntegralGainInput.value = response.integral_gain;
          if (
            preferencePidDerivativeGainInput &&
            response.derivative_gain !== undefined
          )
            preferencePidDerivativeGainInput.value = response.derivative_gain;
          if (
            preferencePidProportionalOnInput &&
            response.proportional_on !== undefined
          )
            preferencePidProportionalOnInput.value = response.proportional_on;
          if (
            preferencePidSampleTimeInput &&
            response.sample_time !== undefined
          ) {
            preferencePidSampleTimeInput.value = response.sample_time;
            lastKnownSampleTime = response.sample_time;
          } else if (preferencePidSampleTimeInput) {
            preferencePidSampleTimeInput.value = lastKnownSampleTime;
          }

          if (response.message_id && response.confirmation_message) {
            lastMessageIdRequiringConfirmation = response.message_id;
            statusArea.textContent = `Action Required: ${
              response.status_message
            } (ID: 0x${response.message_id.toString(16)})`;
            confirmActionButton.style.display = "block";
            rejectActionButton.style.display = "block";
            console.log(
              `Confirmation required for message ID: 0x${response.message_id.toString(
                16
              )}`
            );
          } else {
            hideConfirmModal();
          }

          // Handle state-specific timer display for auto SYNC too
          if (
            response.device_state === 1 &&
            response.target_timer_time !== undefined
          ) {
            // Waiting Timer
            startStateCountdown(response.target_timer_time, "Timer Ends In");
          } else if (
            response.device_state === 3 &&
            response.target_preparing_time !== undefined
          ) {
            // PREPARING
            startStateCountdown(
              response.target_preparing_time,
              "Preparation Finishes In"
            );
          } else {
            clearStateTimer();
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
    const data = event.target?.value || event.data;
    if (!data) return;

    try {
      const dataStr =
        typeof data === "string" ? data : new TextDecoder().decode(data);
      const jsonData = JSON.parse(dataStr);

      // Handle command-specific responses based on 'id' if present
      if (jsonData.id && pendingCommands.has(jsonData.id)) {
        const callback = pendingCommands.get(jsonData.id);
        callback(jsonData); // Resolve the promise/execute callback
        pendingCommands.delete(jsonData.id); // Clean up
      }

      // Handle SYNC command with binary data
      if (jsonData.type === "sync" && jsonData.data) {
        // Decode base64 binary data
        const binaryData = atob(jsonData.data);
        const view = new DataView(new ArrayBuffer(binaryData.length));
        for (let i = 0; i < binaryData.length; i++) {
          view.setUint8(i, binaryData.charCodeAt(i));
        }

        // Parse binary data
        const syncData = {
          current_temperature: view.getFloat32(0, true), // current temp
          target_temperature: view.getFloat32(4, true), // target temp
          pid_output: view.getUint8(8), // output
          sd_card_present: view.getUint8(9) === 1, // sd present
          device_state: view.getUint8(10), // state
          current_step: view.getUint8(11), // step
          process_start_time: view.getUint32(12, true), // started_at
          estimated_time: view.getUint32(16, true), // elapsed_time
          target_timer_time_seconds: view.getUint32(20, true), // remaining_time
          step_time_seconds_start: view.getUint32(24, true), // step_time_seconds_start
          step_time_seconds_estimated: view.getUint32(28, true), // step_time_seconds_estimated
        };

        // Handle timer updates
        updateTimersDisplay({
          started_at: syncData.process_start_time,
          estimated_time: syncData.estimated_time,
          target_timer_time_seconds: syncData.target_timer_time_seconds,
          step_time_seconds_start: syncData.step_time_seconds_start,
          step_time_seconds_estimated: syncData.step_time_seconds_estimated,
        });

        // Update UI based on syncData
        if (syncData.current_temperature !== undefined) {
          currentTemperatureDisplay.textContent = `${syncData.current_temperature.toFixed(
            1
          )}°C`;
        }
        if (syncData.target_temperature !== undefined) {
          targetTemperatureDisplay.textContent = `${syncData.target_temperature.toFixed(
            1
          )}°C`;
        }
        if (syncData.device_state !== undefined) {
          currentStateDisplay.textContent =
            DEVICE_STATES[syncData.device_state] ||
            `Unknown (${syncData.device_state})`;
        }

        // Update temperature graph
        if (
          syncData.current_temperature !== undefined &&
          syncData.target_temperature !== undefined
        ) {
          updateTemperatureGraph(
            syncData.current_temperature,
            syncData.target_temperature,
            syncData.pid_output
          );
        }

        appendToCommandOutput(
          `Received SYNC: ${JSON.stringify(syncData)}`,
          "response"
        );
      } else {
        // Handle other JSON responses
        if (jsonData.id && pendingCommands.has(jsonData.id)) {
          const callback = pendingCommands.get(jsonData.id);
          callback(jsonData);
          pendingCommands.delete(jsonData.id);
        }
        appendToCommandOutput(
          `Received: ${JSON.stringify(jsonData)}`,
          "response"
        );
      }
    } catch (error) {
      console.error("Error parsing telemetry:", error);
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
          if (response.current_temperature !== undefined) {
            currentTemperatureDisplay.textContent = `${response.current_temperature}°C`;
          }
          if (response.target_temperature !== undefined) {
            targetTemperatureDisplay.textContent = `${response.target_temperature}°C`;
          }
          if (response.device_state !== undefined) {
            currentStateDisplay.textContent =
              DEVICE_STATES[response.device_state] ||
              `Unknown (${response.device_state})`;
            // Show/Hide modal based on state
            if (response.device_state === 2) {
              // WAIT_CONFIRM state
              showConfirmModal(
                response.confirmation_message ||
                  "Please confirm the pending action."
              );
            } else {
              hideConfirmModal();
            }
          }
          if (
            response.current_temperature !== undefined &&
            response.target_temperature !== undefined
          ) {
            updateTemperatureGraph(
              response.current_temperature,
              response.target_temperature,
              response.pid_output
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
    autoSyncToggle.checked = true; // Set checkbox to checked by default
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
            appendToCommandOutput(`Command "${cmd}" ACK.`, "response");
          } else {
            appendToCommandOutput(
              `Command "${cmd}" failed or no ACK: ${
                response ? response.error : "Unknown"
              }.`,
              "error"
            );
          }
        });
        if (!success) {
          // sendCommand itself failed (e.g. BLE write error)
          appendToCommandOutput(`Failed to send command "${cmd}".`, "error");
        }
      };

      // SSID (only if not empty)
      if (preferenceSsidInput && preferenceSsidInput.value.trim() !== "") {
        await sendPreferenceCommand(`SSID ${preferenceSsidInput.value.trim()}`);
      }
      // Password (only if not empty - implies user wants to change it)
      if (preferencePasswordInput && preferencePasswordInput.value !== "") {
        // Don't trim password, spaces can be valid
        await sendPreferenceCommand(
          `PASSWORD ${preferencePasswordInput.value}`
        );
        preferencePasswordInput.value = ""; // Clear after sending for security
      }
      // Volume
      if (preferenceVolumeInput && preferenceVolumeInput.value !== "") {
        await sendPreferenceCommand(
          `VOLUME ${parseFloat(preferenceVolumeInput.value)}`
        );
      }
      // Power
      if (preferencePowerInput && preferencePowerInput.value !== "") {
        await sendPreferenceCommand(
          `POWER ${parseFloat(preferencePowerInput.value)}`
        );
      }
      // Hysteresis Temp
      if (
        preferenceHysteresisTemperatureInput &&
        preferenceHysteresisTemperatureInput.value !== ""
      ) {
        await sendPreferenceCommand(
          `HYSTERESIS_TEMP ${parseFloat(
            preferenceHysteresisTemperatureInput.value
          )}`
        );
      }
      // Hysteresis Time
      if (
        preferenceHysteresisTimeInput &&
        preferenceHysteresisTimeInput.value !== ""
      ) {
        await sendPreferenceCommand(
          `HYSTERESIS_TIME ${parseInt(preferenceHysteresisTimeInput.value, 10)}`
        );
      }

      // PID settings (sent as one command)
      if (
        preferencePidProportionalGainInput &&
        preferencePidIntegralGainInput &&
        preferencePidDerivativeGainInput &&
        preferencePidProportionalOnInput &&
        preferencePidSampleTimeInput
      ) {
        const kp = parseFloat(preferencePidProportionalGainInput.value);
        const ki = parseFloat(preferencePidIntegralGainInput.value);
        const kd = parseFloat(preferencePidDerivativeGainInput.value);
        const pOn = parseInt(preferencePidProportionalOnInput.value, 10); // Ensure it's int 0 or 1
        const sampleTime = parseInt(preferencePidSampleTimeInput.value, 10);

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

  // Timer handling functions
  function updateTimersDisplay({
    started_at,
    estimated_time,
    target_timer_time_seconds,
    step_time_seconds_start,
    step_time_seconds_estimated,
  }) {
    const timersContainer = document.getElementById("activeTimersContainer");
    const timerItem = timersContainer.querySelector(".start-timer-item");
    const stepTimerItem = timersContainer.querySelector(".step-timer-item");
    const timerRemainingItem =
      timersContainer.querySelector(".timer-remaining");

    if (started_at) {
      timerItem.querySelector(".start-timer-label").textContent = "Started At";
      timerItem.querySelector(".start-timer-value").textContent = new Date(
        started_at * 1000
      ).toLocaleString();
      timerItem.querySelector(".estimated-timer-label").textContent =
        "Estimated Time";
      timerItem.querySelector(".estimated-timer-value").textContent =
        formatTime(estimated_time);
      timerItem.querySelector(".elapsed-timer-label").textContent =
        "Elapsed Time";
      timerItem.querySelector(".elapsed-timer-value").textContent = formatTime(
        Math.floor(new Date().getTime() / 1000) - started_at
      );
    } else {
      timerItem.querySelector(".start-timer-label").textContent = "Total Time";
      timerItem.querySelector(".start-timer-value").textContent = "00:00:00";
    }
    // this times are UTC
    if (target_timer_time_seconds) {
      timerRemainingItem.querySelector(".timer-remaining-label").textContent =
        "Target Timer";
      timerRemainingItem.querySelector(".timer-remaining-value").textContent =
        new Date(target_timer_time_seconds * 1000).toLocaleString();
    } else {
      // timerItem.querySelector(".timer-remaining-value").textContent = "";
    }

    if (step_time_seconds_start) {
      stepTimerItem.querySelector(".step-timer-label").textContent = "Step";
      stepTimerItem.querySelector(".step-timer-value").textContent = formatTime(
        step_time_seconds_start
      );
    } else {
      stepTimerItem.querySelector(".step-timer-label").textContent = "Step";
      stepTimerItem.querySelector(".step-timer-value").textContent = "00:00:00";
    }
  }

  function formatTime(seconds) {
    if (!seconds || seconds < 0) return "00:00:00";
    const hours = Math.floor(seconds / 3600);
    const minutes = Math.floor((seconds % 3600) / 60);
    const secs = seconds % 60;
    return `${String(hours).padStart(2, "0")}:${String(minutes).padStart(
      2,
      "0"
    )}:${String(secs).padStart(2, "0")}`;
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
