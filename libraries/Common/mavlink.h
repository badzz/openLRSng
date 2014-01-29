#ifndef _Mavlink_Included_
#define _Mavlink_Included_

//#include <stdarg.h>


struct mavlink_RADIO_v10 {
	uint16_t rxerrors;
	uint16_t fixed;
	uint8_t rssi;
	uint8_t remrssi;
	uint8_t txbuf;
	uint8_t noise;
	uint8_t remnoise;
};

// use '3D' for 3DRadio
#define RADIO_SOURCE_SYSTEM '3'
#define RADIO_SOURCE_COMPONENT 'D'

#define MAVLINK_MSG_ID_RADIO 166
#define MAVLINK_RADIO_CRC_EXTRA 21
#define MAV_HEADER_SIZE 6
#define MAV_MAX_PACKET_LENGTH  (MAV_HEADER_SIZE + sizeof(struct mavlink_RADIO_v10) + 2)


static uint8_t g_mavlinkBuffer[MAV_MAX_PACKET_LENGTH];
static uint8_t g_sequenceNumber = 0;

/*
          we use a hand-crafted MAVLink packet based on the following
	  message definition

	  <message name="RADIO" id="166">
	    <description>Status generated by radio</description>
            <field type="uint8_t" name="rssi">local signal strength</field>
            <field type="uint8_t" name="remrssi">remote signal strength</field>
	    <field type="uint8_t" name="txbuf">percentage free space in transmit buffer</field>
	    <field type="uint8_t" name="noise">background noise level</field>
	    <field type="uint8_t" name="remnoise">remote background noise level</field>
	    <field type="uint16_t" name="rxerrors">receive errors</field>
	    <field type="uint16_t" name="fixed">count of error corrected packets</field>
	  </message>
*/


/*
 * Calculates the MAVLink checksum on a packet in parameter buffer 
 * and append it after the data
 */
static void mavlink_crc(uint8_t* buf)
{
	register uint8_t length = buf[1];
    uint16_t sum = 0xFFFF;
	uint8_t i, stoplen;

	stoplen = length + MAV_HEADER_SIZE + 1;

	// MAVLink 1.0 has an extra CRC seed
	buf[length + MAV_HEADER_SIZE] = MAVLINK_RADIO_CRC_EXTRA;

	i = 1;
	while (i<stoplen) {
		register uint8_t tmp;
		tmp = buf[i] ^ (uint8_t)(sum&0xff);
		tmp ^= (tmp<<4);
		sum = (sum>>8) ^ (tmp<<8) ^ (tmp<<3) ^ (tmp>>4);
		i++;
        }

	buf[length+MAV_HEADER_SIZE] = sum&0xFF;
	buf[length+MAV_HEADER_SIZE+1] = sum>>8;
}


// return available space in rx buffer as a percentage
uint8_t	serial_read_space()
{
	uint16_t space = SERIAL_RX_BUFFERSIZE - Serial.available();
	space = (100 * (space / 8)) / (SERIAL_RX_BUFFERSIZE / 8);
	return space;
}


/// send a MAVLink status report packet
void MAVLink_report(FastSerial* serialPort, uint16_t RSSI_local, uint8_t noise, uint16_t rxerrors, uint8_t RSSI_remote)
{
	g_mavlinkBuffer[0] = 254;
	g_mavlinkBuffer[1] = sizeof(struct mavlink_RADIO_v10);
	g_mavlinkBuffer[2] = g_sequenceNumber++;
	g_mavlinkBuffer[3] = RADIO_SOURCE_SYSTEM;
	g_mavlinkBuffer[4] = RADIO_SOURCE_COMPONENT;
	g_mavlinkBuffer[5] = MAVLINK_MSG_ID_RADIO;


	// NOTE: 
	// In mission planner, the Link quality is a percentage of the number
	// of good packets received
	// to the number of packets missed (detected by mavlink seq no.)
	// mission planner does disregard packets with '3D' in header for this calculation

	struct mavlink_RADIO_v10 *m = (struct mavlink_RADIO_v10 *)&g_mavlinkBuffer[MAV_HEADER_SIZE];
	m->rxerrors = rxerrors; // errors.rx_errors;
	m->fixed    = 0; //errors.corrected_packets;
	m->txbuf    = serial_read_space(); //serial_read_space();
	m->rssi     = RSSI_local; //statistics.average_rssi;
	m->remrssi  = RSSI_remote; //remote_statistics.average_rssi;
	m->noise    = noise; //statistics.average_noise;
	m->remnoise = 0; //remote_statistics.average_noise;

	mavlink_crc(g_mavlinkBuffer);

	if (serialPort->txspace() >= sizeof(g_mavlinkBuffer)) 		// don't cause an overflow
	{
		serialPort->write(g_mavlinkBuffer, sizeof(g_mavlinkBuffer));
	}
}

#endif