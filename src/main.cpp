#include <hardware/pio.h>
#include <inttypes.h>
#include <stdio.h>
#include <pico/stdlib.h>
#include <pico/cyw43_arch.h>
#include <pico/multicore.h>

#include "btstack_config.h"
#include "btstack.h"
#include "btstack_run_loop.h"

#include "N64Console.hpp"
#include "n64_definitions.h"


#define JOYBUS_PIN 0
n64_report_t n64_report = default_n64_report;


#define MAX_ATTRIBUTE_VALUE_SIZE 300
static const char * remote_addr_string = "90:89:5F:2A:1C:FB";
static bd_addr_t remote_addr;
static btstack_packet_callback_registration_t hci_event_callback_registration;
static uint8_t hid_descriptor_storage[MAX_ATTRIBUTE_VALUE_SIZE];
static uint16_t hid_host_cid = 0;
static hid_protocol_mode_t hid_host_report_mode = HID_PROTOCOL_MODE_REPORT_WITH_FALLBACK_TO_BOOT;
static bool get_report_sent = false;

static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);

static void hid_host_setup(void){
    l2cap_init();
    hid_host_init(hid_descriptor_storage, sizeof(hid_descriptor_storage));
    hid_host_register_packet_handler(packet_handler);
    gap_set_default_link_policy_settings(LM_LINK_POLICY_ENABLE_SNIFF_MODE | LM_LINK_POLICY_ENABLE_ROLE_SWITCH);
    hci_set_master_slave_policy(HCI_ROLE_MASTER);
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);
    setvbuf(stdin, NULL, _IONBF, 0);
}

static void hid_host_handle_interrupt_report(const uint8_t * report, uint16_t report_len){
    if (report_len < 1) return;
    if (*report != 0xa1) return; 
    
    report++;
    report_len--;

    // Support simple 10-bytes report _and_ full 79-bytes report
    uint8_t report_protocol_code = report[0];
    if (report_protocol_code == 0x11) { // Full report
        // Skip two bytes
        report += 2;
        report_len -= 2;
    }

    // Parse controller report
    uint8_t axis_L_x = report[1];
    uint8_t axis_L_y = report[2];
    uint8_t axis_R_x = report[3];
    uint8_t axis_R_y = report[4];
    uint8_t buttons_1 = report[5];
    uint8_t buttons_2 = report[6];
    uint8_t dpad = buttons_1 & 0x0f;
    uint8_t trigger_L = report[8];
    uint8_t trigger_R = report[9];

    // Update N64 controller report
    n64_report.dpad_right = (dpad == 0x01 || dpad == 0x02 || dpad == 0x03);
    n64_report.dpad_left = (dpad == 0x05 || dpad == 0x06 || dpad == 0x07);
    n64_report.dpad_down = (dpad == 0x03 || dpad == 0x04 || dpad == 0x05);
    n64_report.dpad_up = (dpad == 0x00 || dpad == 0x01 || dpad == 0x07);
    n64_report.start = (buttons_2 & 0x20) != 0; // Options
    n64_report.z = (buttons_2 & 0x04) != 0; // L2
    n64_report.b = (buttons_1 & 0x10) != 0; // Square
    n64_report.a = (buttons_1 & 0x20) != 0; // Cross
    n64_report.c_right = (axis_R_x > 156);
    n64_report.c_left = (axis_R_x < 100);
    n64_report.c_down = (axis_R_y > 156);
    n64_report.c_up = (axis_R_y < 100);
    n64_report.r = (buttons_2 & 0x02) != 0; // R1
    n64_report.l = (buttons_2 & 0x01) != 0; // L1
    // Handle deadzone?
    n64_report.stick_x = (axis_L_x >= 125 && axis_L_x <= 131) ? 0 : (axis_L_x - 128);
    n64_report.stick_y = (axis_L_y >= 125 && axis_L_y <= 131) ? 0 : ((255 - axis_L_y) - 128);
}

static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)
{
    UNUSED(channel);
    UNUSED(size);

    uint8_t   event;
    bd_addr_t event_addr;
    uint8_t   status;

    switch (packet_type) {
		case HCI_EVENT_PACKET:
            event = hci_event_packet_get_type(packet);
            
            switch (event) {

                /* @text When BTSTACK_EVENT_STATE with state HCI_STATE_WORKING
                 * is received and the example is started in client mode, the remote SDP HID query is started.
                 */
                case BTSTACK_EVENT_STATE:
                    if (btstack_event_state_get_state(packet) == HCI_STATE_WORKING){
                        status = hid_host_connect(remote_addr, hid_host_report_mode, &hid_host_cid);
                        if (status != ERROR_CODE_SUCCESS){
                            printf("HID host connect failed, status 0x%02x.\n", status);
                        }
                    }
                    break;
                
                case HCI_EVENT_PIN_CODE_REQUEST:
					// inform about pin code request
                    printf("Pin code request - using '0000'\n");
                    hci_event_pin_code_request_get_bd_addr(packet, event_addr);
                    gap_pin_code_response(event_addr, "0000");
					break;
                
                case HCI_EVENT_USER_CONFIRMATION_REQUEST:
                    // inform about user confirmation request
                    printf("SSP User Confirmation Request with numeric value '%"PRIu32"'\n", little_endian_read_32(packet, 8));
                    printf("SSP User Confirmation Auto accept\n");
                    break;
                
                case HCI_EVENT_HID_META:
                    switch (hci_event_hid_meta_get_subevent_code(packet)){

                        case HID_SUBEVENT_INCOMING_CONNECTION:
                            // There is an incoming connection: we can accept it or decline it.
                            // The hid_host_report_mode in the hid_host_accept_connection function 
                            // allows the application to request a protocol mode. 
                            // For available protocol modes, see hid_protocol_mode_t in btstack_hid.h file. 
                            hid_host_accept_connection(hid_subevent_incoming_connection_get_hid_cid(packet), hid_host_report_mode);
                            break;
                        
                        case HID_SUBEVENT_CONNECTION_OPENED:
                            // The status field of this event indicates if the control and interrupt
                            // connections were opened successfully.
                            status = hid_subevent_connection_opened_get_status(packet);
                            if (status != ERROR_CODE_SUCCESS) {
                                printf("Connection failed, status 0x%02x\n", status);
                                hid_host_cid = 0;
                                return;
                            }
                            hid_host_cid = hid_subevent_connection_opened_get_hid_cid(packet);
                            printf("HID Host connected: %d\n", hid_host_cid);
                            break;

                        case HID_SUBEVENT_DESCRIPTOR_AVAILABLE:
                            // This event will follows HID_SUBEVENT_CONNECTION_OPENED event. 
                            // For incoming connections, i.e. HID Device initiating the connection,
                            // the HID_SUBEVENT_DESCRIPTOR_AVAILABLE is delayed, and some HID  
                            // reports may be received via HID_SUBEVENT_REPORT event. It is up to 
                            // the application if these reports should be buffered or ignored until 
                            // the HID descriptor is available.
                            status = hid_subevent_descriptor_available_get_status(packet);
                            if (status == ERROR_CODE_SUCCESS){
                                printf("HID Descriptor available, please start typing.\n");
                            } else {
                                printf("Cannot handle input report, HID Descriptor is not available, status 0x%02x\n", status);
                            }
                            break;

                        case HID_SUBEVENT_REPORT:
                            // Handle input report
                            hid_host_handle_interrupt_report(hid_subevent_report_get_report(packet), hid_subevent_report_get_report_len(packet));
                            // Enable full reports
                            if (!get_report_sent) {
                                printf("Sending GET REPORT: FEATURE 0x02. hid_host_cid = %d\n", hid_host_cid);
                                status = hid_host_send_get_report(hid_host_cid, HID_REPORT_TYPE_FEATURE, 0x02);
                                if (status != ERROR_CODE_SUCCESS) {
                                    printf("Failed to send GET REPORT, status 0x%02x\n", status);
                                    break;
                                }
                                get_report_sent = true;
                                printf("Sent.\n");
                            }
                            break;

                        case HID_SUBEVENT_SET_PROTOCOL_RESPONSE:
                            // For incoming connections, the library will set the protocol mode of the
                            // HID Device as requested in the call to hid_host_accept_connection. The event 
                            // reports the result. For connections initiated by calling hid_host_connect, 
                            // this event will occur only if the established report mode is boot mode.
                            status = hid_subevent_set_protocol_response_get_handshake_status(packet);
                            if (status != HID_HANDSHAKE_PARAM_TYPE_SUCCESSFUL){
                                printf("Error set protocol, status 0x%02x\n", status);
                                break;
                            }
                            switch ((hid_protocol_mode_t)hid_subevent_set_protocol_response_get_protocol_mode(packet)){
                                case HID_PROTOCOL_MODE_BOOT:
                                    printf("Protocol mode set: BOOT.\n");
                                    break;  
                                case HID_PROTOCOL_MODE_REPORT:
                                    printf("Protocol mode set: REPORT.\n");
                                    break;
                                default:
                                    printf("Unknown protocol mode.\n");
                                    break; 
                            }
                            break;

                        case HID_SUBEVENT_CONNECTION_CLOSED:
                            // The connection was closed.
                            hid_host_cid = 0;
                            printf("HID Host disconnected.\n");
                            break;
                        
                        default:
                            break;
                    }
                    break;
                default:
                    break;
            }
            break;
        default:
            break;
    }
}

void core1_entry() {
    printf("[core1] Initializing joybus on core 1\n");
    N64Console *console = new N64Console(JOYBUS_PIN, pio0);
    // Handle poll commands
    printf("[core1] Entering loop\n");
    while(true) {
        //printf("[core1] Waiting for POLL command from console...\n");
        console->WaitForPoll();
        //printf("[core1] sending report\n");
        console->SendReport(&n64_report);
    }
}

int main(void) {
    set_sys_clock_khz(130'000, true);
    stdio_init_all();

    printf("[core0] Starting pico-ntroller64\n");

    printf("[core0] Spawn joybus process on core 1\n");
    multicore_launch_core1(core1_entry);
    
    printf("[core0] Init bluetooth stack\n");
    if (cyw43_arch_init()) {
        printf("[core0] cyw43 init failed\n");
        return -1;
    }
    sscanf_bd_addr(remote_addr_string, remote_addr);
    hid_host_setup();
    hci_power_control(HCI_POWER_ON);
    btstack_run_loop_execute();

    return 0;
}
