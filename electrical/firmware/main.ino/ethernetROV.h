#include <Ethernet.h>
#include <EthernetUDP.h>

// MAC address determined by hardware on Ethernet Shield
// IP address needs to be on same network as control computer
byte mac[] = {0x90, 0xAD, 0xD2, 0x0D, 0x8B, 0x23};
IPAddress ip(192, 168, 1, 177);

// Local Port to listen on
unsigned int localPort = 8888;

// Buffers for receiving and sending data
char packetBuffer[UDP_TX_PACKET_MAX_SIZE];
char ReplyBuffer[UDP_RX_PACKET_MAX_SIZE];


EthernetUDP Udp;

void ethernetSetup() {
	Ethernt.setup(mac, ip);
	Udp.begin(localPort);
}

int ethernetRead() {
	if (Udp.parsePacket()) {
		Udp.read(packetBuffer, UDP_TX_PACKET_MAX_SIZE);

		// Administrative Information
		commanded_state				= packetBuffer[0];
		counter						= packetBuffer[1];

		// Thruster Commands
		motorCommand.frontLeft		= packetBuffer[2];
		motorCommand.frontRight		= packetBuffer[3];
		motorCommand.backLeft		= packetBuffer[4];
		motorCommand.backRight		= packetBuffer[5];
		motorCommand.vertLeft		= packetBuffer[6];
		motorCommand.vertRight		= packetBuffer[7];

		// Arm Positions
		armCommand.elbow			= packetBuffer[8];
		armCommand.wrist			= packetBuffer[9];
		armCommand.grasp			= packetBuffer[10];

		return 1;
	}
	else {
		return 0;
	}
}

void ethernetWrite() {
	Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());

	// Response State
	Udp.write(counter);
	Udp.write(accelData.x)
}