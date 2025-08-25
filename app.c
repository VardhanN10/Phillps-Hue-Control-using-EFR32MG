/***************************************************************************//**
 * @file app.c
 * @brief Callbacks implementation and application specific code.
 *******************************************************************************
 * # License
 * <b>Copyright 2021 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * The licensor of this software is Silicon Laboratories Inc. Your use of this
 * software is governed by the terms of Silicon Labs Master Software License
 * Agreement (MSLA) available at
 * www.silabs.com/about-us/legal/master-software-license-agreement. This
 * software is distributed to you in Source Code format and is governed by the
 * sections of the MSLA applicable to Source Code.
 *
 ******************************************************************************/

/* -----------------------------------------------------------------------------
 *  Module: Philips Hue control (EFR32MG, Zigbee)
 *
 *  Overview
 *  - BTN0: Create the Zigbee network and open it for joining (event-driven).
 *  - BTN1: Send ZCL On/Off Toggle to the Hue bulb using its captured short ID.
 *
 *  Data Flow
 *  - Trust Center join callback stores the Hue bulb short ID after join.
 *  - Main tick polls button flags -> dispatches actions (create network / toggle).
 *
 *  Notes
 *  - The code below intentionally retains original behaviour and print messages.
 *  - CLI equivalents (for reference only):
 *      plugin network-creator start 1
 *      plugin network-creator-security open-network
 * ---------------------------------------------------------------------------*/

// === Includes ================================================================ //

#include "app/framework/include/af.h"
#include "sl_simple_button.h"
#include "sl_simple_button_instances.h"
#include "app/framework/plugin/network-creator/network-creator.h"
#include "app/framework/plugin/network-creator-security/network-creator-security.h"

// === Configuration / Mappings ===============================================

/*** Variables for Button Change Interrupts**/
/**
 * @brief Logical indices used by Simple Button instances.
 * BTN0: triggers network create/open event.
 * BTN1: sends On/Off Toggle to the stored Hue node ID.
 */

#define BUTTON0 0
#define BUTTON1 1

/** @brief Button flags set on RELEASE; consumed in emberAfMainTickCallback(). */
volatile bool button0Pressed;
volatile bool button1Pressed;



/*** Variables for Phillps hue bulb**/
/**
 * @brief Hue bulb endpoint used for ZCL On/Off cluster (typical = 11).
 * @note Keep consistent with ZAP configuration.
 */
#define HUE_BULB_ENDPOINT 11
/** @brief Short ID of the Hue bulb; captured in Trust Center join callback. */
EmberNodeId hueNodeId = EMBER_NULL_NODE_ID;


/** EVENT declare**/
/**
 * @brief Event used to create/open the network when BTN0 is pressed.
 * createNetworkFunc() performs the action when the event fires.
 */

static sl_zigbee_event_t createNetwork;
static void createNetworkFunc(sl_zigbee_event_t *event);


//*AUTO GEN CODE**//
/** @brief Complete network steering.
 *
 * This callback is fired when the Network Steering plugin is complete.
 *
 * @param status On success this will be set to EMBER_SUCCESS to indicate a
 * network was joined successfully. On failure this will be the status code of
 * the last join or scan attempt. Ver.: always
 *
 * @param totalBeacons The total number of 802.15.4 beacons that were heard,
 * including beacons from different devices with the same PAN ID. Ver.: always
 * @param joinAttempts The number of join attempts that were made to get onto
 * an open Zigbee network. Ver.: always
 *
 * @param finalState The finishing state of the network steering process. From
 * this, one is able to tell on which channel mask and with which key the
 * process was complete. Ver.: always
 */
void emberAfPluginNetworkSteeringCompleteCallback(EmberStatus status,
                                                  uint8_t totalBeacons,
                                                  uint8_t joinAttempts,
                                                  uint8_t finalState)
{
  sl_zigbee_app_debug_println("%s network %s: 0x%02X", "Join", "complete", status);
}

/** @brief
 *
 * Application framework equivalent of ::emberRadioNeedsCalibratingHandler
 */
void emberAfRadioNeedsCalibratingCallback(void)
{
  sl_mac_calibrate_current_channel();
}

////AUTO GEN CODE- END/////////

// === Buttons ================================================================

/**
 * @brief Simple Button change callback.
 *
 * Sets button flags on RELEASE; actual work is done in the main tick.
 * - BUTTON0 -> create/open network (via event).
 * - BUTTON1 -> toggle Hue bulb using stored short ID.
 */

void sl_button_on_change(const sl_button_t *handle)
{
  if (SL_SIMPLE_BUTTON_INSTANCE(BUTTON0) == handle){
      if (sl_button_get_state(handle) == SL_SIMPLE_BUTTON_RELEASED){
          button0Pressed = true;
      }
  }
  if (SL_SIMPLE_BUTTON_INSTANCE(BUTTON1) == handle){
      if (sl_button_get_state(handle) == SL_SIMPLE_BUTTON_RELEASED){
          button1Pressed= true;
      }
  }
}

// === Application actions =====================================================

/**
 * @brief Dispatches actions based on button flags.
 *
 * - If BTN0 was released: schedule createNetwork event.
 * - If BTN1 was released: send ZCL Toggle to Hue bulb (if joined).
 *
 * @note Keeps original behaviour and print messages.
 */

void sendMessage()
{
EmberStatus status;

if (button0Pressed)
  {

    sl_zigbee_event_set_active(&createNetwork); // Setting event On active
    button0Pressed = false; // Resetting the Flag
  }

if (button1Pressed)
  {
    if (hueNodeId == EMBER_NULL_NODE_ID) {
      emberAfCorePrintln("Hue bulb yet to join the network");
    }
    else {
    emberAfFillCommandOnOffClusterToggle();

       emberAfCorePrintln("Button1 is pressed");
           emberAfCorePrintln("Command is zcl on-off Toggle");
           emberAfSetCommandEndpoints(emberAfPrimaryEndpoint(), HUE_BULB_ENDPOINT);
           status = emberAfSendCommandUnicast(EMBER_OUTGOING_DIRECT, hueNodeId);

    status = emberAfSendCommandUnicast(EMBER_OUTGOING_DIRECT, 0x0000);
    if (status == EMBER_SUCCESS)
      {
        emberAfCorePrintln("Command is successfully sent");
      }
    else
      {
        emberAfCorePrintln("Failed to send");
        emberAfCorePrintln("Status code: 0x%x", status);
      }
    button1Pressed = false; // Resetting the Flag
  }
}
}

// === Event handler ===========================================================

/**
 * @brief Event function that creates the network and (indirectly) opens join.
 *
 * Behaviour:
 * - If not in a network: start Network Creator (centralized).
 * - If already in a network: print status (original behaviour).
 *
 * @note Opening the network (join window) happens when stack goes up
 *       inside emberAfStackStatusCallback().
 */
static void createNetworkFunc(sl_zigbee_event_t *event)
{

  EmberNetworkStatus networkStatus;
  EmberStatus status;
  networkStatus = emberAfNetworkState();/** @brief Indicate whether the stack is currently up.
   *
   *   Returns true if the stack is joined to a network and
   *   ready to send and receive messages.  This reflects only the state
   *   of the local node; it does not indicate whether other nodes are
   *   able to communicate with this node.
   *
   * @return True if the stack is up, false otherwise.
   */
  if(networkStatus == EMBER_NO_NETWORK)
    {
      status = emberAfPluginNetworkCreatorStart(true);
      if(status == EMBER_SUCCESS){
           emberAfCorePrintln("Network Creation success");

       }else{
           emberAfCorePrintln("Network Creation failed");
       }
    }
  else{
      emberAfCorePrintln("Already In a network");
  }

}
// === Stack callbacks =========================================================

/**
 * @brief Stack status change callback.
 *
 * On EMBER_NETWORK_UP, opens the network for joining.
 * Equivalent to CLI: plugin network-creator-security open-network
 */
void emberAfStackStatusCallback(EmberStatus status)
{

  if (status == EMBER_NETWORK_UP) {
    // Equivalent to: plugin network-creator-security open-network
    EmberStatus sec = emberAfPluginNetworkCreatorSecurityOpenNetwork();
    emberAfCorePrintln("open-network -> 0x%02X", sec);
  }
}

/**
 * @brief Main periodic tick.
 *
 * Checks button flags and triggers the corresponding actions.
 */
void emberAfMainTickCallback(void){

  if (button0Pressed || button1Pressed)
    {
      sendMessage();
    }
}

/**@breif Main Init Call Back
 *
 * This function is called from the application's main function. It gives the application
 * a chance to do any initialization required at system startup. Any code that you would normally
 *  put into the top of the application's main() routine should be put into this function.
 *   This is called before the clusters, plugins, and the network are initialized so some functionality
 *    is not yet available.
 */
void emberAfMainInitCallback(void)
{

  sl_zigbee_event_init(&createNetwork, createNetworkFunc);
}

/**
 * @brief Trust Center join callback.
 *
 * Captures and stores the short ID of newly joined devices (Hue bulb).
 * The stored short ID is used later for direct unicast control.
 *
 * @note Original print uses the provided format string exactly as in source.
 */
void emberAfTrustCenterJoinCallback(EmberNodeId newNodeId,
                                    EmberEUI64 newNodeEui64,
                                    EmberNodeId parentOfNewNode,
                                    EmberDeviceUpdate status,
                                    EmberJoinDecision decision)
{
    hueNodeId = newNodeId;
    emberAfCorePrintln("Captured Hue short ID: ", hueNodeId);
}


