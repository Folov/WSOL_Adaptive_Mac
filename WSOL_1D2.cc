#include "ns3/core-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/yans-wifi-channel.h"
#include "ns3/wifi-net-device.h"
#include "ns3/qos-txop.h"
#include "ns3/wifi-mac.h"
#include "ns3/ssid.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/wifi-remote-station-manager.h"
#include "ns3/mac-tx-middle.h"
#include "ns3/mac-low.h"
// For flowmonitor
#include "ns3/flow-monitor-module.h"
#include "ns3/openflow-module.h"
#include "ns3/log.h"
#include "ns3/olsr-helper.h"
// For gnuplot and Gnuplot2Ddatabase
#include "ns3/stats-module.h"
#include "ns3/random-variable-stream.h"
#include "ns3/netanim-module.h"
#include <iostream>
#include <fstream>
#include <vector>
// For myOnOffApplication/Helper
#include "ns3/attribute.h"
#include "ns3/address.h"
#include "ns3/application-container.h"
#include "ns3/event-id.h"
#include "ns3/data-rate.h"
#include "ns3/ptr.h"
#include "ns3/traced-callback.h"
#include "ns3/inet-socket-address.h"
#include "ns3/packet-socket-address.h"
#include "ns3/uinteger.h"
#include "ns3/names.h"
#include "ns3/string.h"

int count_packets=0;
// int data_packetsize[48];
// double data_interval[48];
// myOnOffApplication.h, change from onoff-application.h in ns3
namespace ns3 {
	class Address;
	class RandomVariableStream;
	class Socket;

	class myOnOffApplication : public Application
	{
	public:
		myOnOffApplication();
		virtual ~myOnOffApplication();
		static TypeId GetTypeId (void);
		void SetMaxBytes (uint32_t maxBytes);
		Ptr<Socket> GetSocket (void) const;
		int64_t AssignStreams (int64_t stream);
	protected:
		virtual void DoDispose (void);
	private:
		virtual void StartApplication (void);
		virtual void StopApplication (void);
		void CancelEvents ();
		void StartSending ();
		void StopSending ();
		void SendPacket ();	
		
		Ptr<Socket>     m_socket;       //!< Associated socket
		Address         m_peer;         //!< Peer address
		bool            m_connected;    //!< True if connected
		Ptr<RandomVariableStream>  m_onTime;       //!< rng for On Time
		Ptr<RandomVariableStream>  m_offTime;      //!< rng for Off Time
		Ptr<RandomVariableStream>  m_interval;    
		Ptr<RandomVariableStream>  m__packetsize;
		DataRate        m_cbrRate;      //!< Rate that data is generated
		DataRate        m_cbrRateFailSafe;      //!< Rate that data is generated (check copy)
		uint32_t        m_pktSize;      //!< Size of packets
		uint32_t        m_residualBits; //!< Number of generated, but not sent, bits
		Time            m_lastStartTime; //!< Time last packet sent
		uint32_t        m_maxBytes;     //!< Limit total number of bytes sent
		uint32_t        m_totBytes;     //!< Total bytes sent so far
		EventId         m_startStopEvent;     //!< Event id for next start or stop event
		EventId         m_sendEvent;    //!< Event id of pending "send packet" event
		TypeId          m_tid;          //!< Type of the socket used

		TracedCallback<Ptr<const Packet> > m_txTrace;

	private:
		 void ScheduleNextTx ();
		 void ScheduleStartEvent ();
		 void ScheduleStopEvent ();
		 void ConnectionSucceeded (Ptr<Socket> socket);
		 void ConnectionFailed (Ptr<Socket> socket);	
	};
} // namespace ns3

// myOnOffApplication.cc, change from onoff-application.cc in ns3
namespace ns3 {
	NS_LOG_COMPONENT_DEFINE ("myOnOffApplication");
	NS_OBJECT_ENSURE_REGISTERED (myOnOffApplication);
	TypeId
	myOnOffApplication::GetTypeId (void)
	{
		static TypeId tid = TypeId ("ns3::myOnOffApplication")
		.SetParent<Application> ()
		.SetGroupName("Applications")
		.AddConstructor<myOnOffApplication> ()
		.AddAttribute ("DataRate", "The data rate in on state.",
						 DataRateValue (DataRate ("500kb/s")),
						 MakeDataRateAccessor (&myOnOffApplication::m_cbrRate),
						 MakeDataRateChecker ())
		.AddAttribute ("PacketSize", "The size of packets sent in on state",
						 StringValue ("ns3::ConstantRandomVariable[Constant=512]"),
						 MakePointerAccessor (&myOnOffApplication::m__packetsize),
						 MakePointerChecker <RandomVariableStream>())
		.AddAttribute ("Interval", "The time between packets sent in on state",
						 StringValue ("ns3::ConstantRandomVariable[Constant=0]"),
						 MakePointerAccessor (&myOnOffApplication::m_interval),
						 MakePointerChecker <RandomVariableStream>())
		.AddAttribute ("Remote", "The address of the destination",
						 AddressValue (),
						 MakeAddressAccessor (&myOnOffApplication::m_peer),
						 MakeAddressChecker ())
		.AddAttribute ("OnTime", "A RandomVariableStream used to pick the duration of the 'On' state.",
						 StringValue ("ns3::ConstantRandomVariable[Constant=1.0]"),
						 MakePointerAccessor (&myOnOffApplication::m_onTime),
						 MakePointerChecker <RandomVariableStream>())
		.AddAttribute ("OffTime", "A RandomVariableStream used to pick the duration of the 'Off' state.",
						 StringValue ("ns3::ConstantRandomVariable[Constant=1.0]"),
						 MakePointerAccessor (&myOnOffApplication::m_offTime),
						 MakePointerChecker <RandomVariableStream>())
		.AddAttribute ("MaxBytes",
						 "The total number of bytes to send. Once these bytes are sent, "
						 "no packet is sent again, even in on state. The value zero means "
						 "that there is no limit.",
						 UintegerValue (0),
						 MakeUintegerAccessor (&myOnOffApplication::m_maxBytes),
						 MakeUintegerChecker<uint32_t> ())
		.AddAttribute ("Protocol", "The type of protocol to use.",
						 TypeIdValue (UdpSocketFactory::GetTypeId ()),
						 MakeTypeIdAccessor (&myOnOffApplication::m_tid),
						 MakeTypeIdChecker ())
		.AddTraceSource ("Tx", "A new packet is created and is sent",
						 MakeTraceSourceAccessor (&myOnOffApplication::m_txTrace),
						 "ns3::Packet::TracedCallback")
		;
		return tid;
	}
	myOnOffApplication::myOnOffApplication ()
		: m_socket (0),
		m_connected (false),
		m_residualBits (0),
		m_lastStartTime (Seconds (0)),
		m_totBytes (0)
	{ NS_LOG_FUNCTION (this); }
	myOnOffApplication::~myOnOffApplication()
	{ NS_LOG_FUNCTION (this); }

	void myOnOffApplication::SetMaxBytes (uint32_t maxBytes)
	{
		NS_LOG_FUNCTION (this << maxBytes);
		m_maxBytes = maxBytes;
	}
	Ptr<Socket> myOnOffApplication::GetSocket (void) const
	{
		NS_LOG_FUNCTION (this);
		return m_socket;
	}
	int64_t myOnOffApplication::AssignStreams (int64_t stream) // for random numbers
	{
		NS_LOG_FUNCTION (this << stream);
		m_onTime->SetStream (stream);
		m_offTime->SetStream (stream + 1);
		m_interval->SetStream (stream + 2);
		m__packetsize->SetStream (stream + 3);
		return 4;
	}
	void myOnOffApplication::DoDispose (void)
	{
		NS_LOG_FUNCTION (this);
		m_socket = 0;
		Application::DoDispose ();
	}
	// Application Methods
	void myOnOffApplication::StartApplication () // Called at time specified by Start
	{
		NS_LOG_FUNCTION (this);
		// Create the socket if not already
		if (!m_socket)
		{
			m_socket = Socket::CreateSocket (GetNode (), m_tid);
			if (Inet6SocketAddress::IsMatchingType (m_peer))
			m_socket->Bind6 ();
			else if (InetSocketAddress::IsMatchingType (m_peer) ||
					 PacketSocketAddress::IsMatchingType (m_peer))
			m_socket->Bind ();
			m_socket->Connect (m_peer);
			m_socket->SetAllowBroadcast (true);
			m_socket->ShutdownRecv ();
			m_socket->SetConnectCallback (
			MakeCallback (&myOnOffApplication::ConnectionSucceeded, this),
			MakeCallback (&myOnOffApplication::ConnectionFailed, this));
		}
		m_cbrRateFailSafe = m_cbrRate;
		// Insure no pending event
		CancelEvents ();
		// If we are not yet connected, there is nothing to do here
		// The ConnectionComplete upcall will start timers at that time
		//if (!m_connected) return;
		ScheduleStartEvent ();
	}
	void myOnOffApplication::StopApplication () // Called at time specified by Stop
	{
		NS_LOG_FUNCTION (this);
		CancelEvents ();
		if(m_socket != 0)
		m_socket->Close ();
		else
		NS_LOG_WARN ("myOnOffApplication found null socket to close in StopApplication");
	}
	void myOnOffApplication::CancelEvents ()
	{
		NS_LOG_FUNCTION (this);
		if (m_sendEvent.IsRunning () && m_cbrRateFailSafe == m_cbrRate )
		{ // Cancel the pending send packet event
			// Calculate residual bits since last packet sent
			Time delta (Simulator::Now () - m_lastStartTime);
			int64x64_t bits = delta.To (Time::S) * m_cbrRate.GetBitRate ();
			m_residualBits += bits.GetHigh ();
		}
		m_cbrRateFailSafe = m_cbrRate;
		Simulator::Cancel (m_sendEvent);
		Simulator::Cancel (m_startStopEvent);
	}
	// Event handlers
	void myOnOffApplication::StartSending ()
	{
		NS_LOG_FUNCTION (this);
		m_lastStartTime = Simulator::Now ();
		ScheduleNextTx ();  // Schedule the send packet event
		ScheduleStopEvent ();
	}
	void myOnOffApplication::StopSending ()
	{
		NS_LOG_FUNCTION (this);
		CancelEvents ();
		ScheduleStartEvent ();
	}
	void myOnOffApplication::ScheduleNextTx ()
	{
		NS_LOG_FUNCTION (this);
		if (m_maxBytes == 0 || m_totBytes < m_maxBytes)
		{
			m_pktSize = m__packetsize->GetValue();   // 添加这一个数据包大小控制语句
			// data_packetsize[count_packets] = m_pktSize;
			int16_t bits = m_pktSize * 8 - m_residualBits;
			if (bits < 0)	// m_pktSize是随机变量，不能保证发送数据一定大于m_residualBits
			bits = 0;
			NS_LOG_LOGIC ("bits = " << bits);

			double myinterval = m_interval->GetValue();
			std::cout << "The interval is "<< myinterval<< std::endl;     
			// data_interval[count_packets] = myinterval;
			count_packets++;
			Time nextTime (Seconds (myinterval));
			if (myinterval == 0)
			Time nextTime (Seconds (bits / static_cast<double>(m_cbrRate.GetBitRate ()))); // Time till next packet
			else
			Time nextTime (Seconds (myinterval)); // Time till next packet
			// Time nextTime (Seconds (bits /
			//                         static_cast<double>(m_cbrRate.GetBitRate ()) + myinterval)); // Time till next packet
			NS_LOG_LOGIC ("nextTime = " << nextTime);
			m_sendEvent = Simulator::Schedule (nextTime,
											 &myOnOffApplication::SendPacket, this);     
			std::cout << count_packets <<" into ScheduleNextTx ()"<<std::endl;
		}
		else
		{ // All done, cancel any pending events
			StopApplication ();
		}
	}
	void myOnOffApplication::ScheduleStartEvent ()
	{  // Schedules the event to start sending data (switch to the "On" state)
		NS_LOG_FUNCTION (this);
		Time offInterval = Seconds (m_offTime->GetValue ());
		NS_LOG_LOGIC ("start at " << offInterval);
		m_startStopEvent = Simulator::Schedule (offInterval, &myOnOffApplication::StartSending, this);
	}
	void myOnOffApplication::ScheduleStopEvent ()
	{  // Schedules the event to stop sending data (switch to "Off" state)
		NS_LOG_FUNCTION (this);
		Time onInterval = Seconds (m_onTime->GetValue ());
		NS_LOG_LOGIC ("stop at " << onInterval);
		m_startStopEvent = Simulator::Schedule (onInterval, &myOnOffApplication::StopSending, this);
	}
	void myOnOffApplication::SendPacket ()
	{
		NS_LOG_FUNCTION (this);
		NS_ASSERT (m_sendEvent.IsExpired ());
		Ptr<Packet> packet = Create<Packet> (m_pktSize);
		m_txTrace (packet);
		m_socket->Send (packet);
		m_totBytes += m_pktSize;
		if (InetSocketAddress::IsMatchingType (m_peer))
		{
			NS_LOG_INFO ("At time " << Simulator::Now ().GetSeconds ()
						 << "s on-off application sent "
						 <<  packet->GetSize () << " bytes to "
						 << InetSocketAddress::ConvertFrom(m_peer).GetIpv4 ()
						 << " port " << InetSocketAddress::ConvertFrom (m_peer).GetPort ()
						 << " total Tx " << m_totBytes << " bytes");
		}
		else if (Inet6SocketAddress::IsMatchingType (m_peer))
		{
			NS_LOG_INFO ("At time " << Simulator::Now ().GetSeconds ()
						 << "s on-off application sent "
						 <<  packet->GetSize () << " bytes to "
						 << Inet6SocketAddress::ConvertFrom(m_peer).GetIpv6 ()
						 << " port " << Inet6SocketAddress::ConvertFrom (m_peer).GetPort ()
						 << " total Tx " << m_totBytes << " bytes");
		}
		m_lastStartTime = Simulator::Now ();
		m_residualBits = 0;
		ScheduleNextTx ();
	}
	void myOnOffApplication::ConnectionSucceeded (Ptr<Socket> socket)
	{
		NS_LOG_FUNCTION (this << socket);
		m_connected = true;
	}
	void myOnOffApplication::ConnectionFailed (Ptr<Socket> socket)
	{
		NS_LOG_FUNCTION (this << socket);
	}
} // namespace ns3

// myOnOffHelper.h, change from on-off-helper.h in ns3
namespace ns3 {
	class DataRate;
	class myOnOffHelper
	{
	public:
		myOnOffHelper (std::string protocol, Address address);
		void SetAttribute (std::string name, const AttributeValue &value);
		void SetConstantRate (DataRate dataRate, uint32_t packetSize = 512);
		ApplicationContainer Install (NodeContainer c) const;
		ApplicationContainer Install (Ptr<Node> node) const;
		ApplicationContainer Install (std::string nodeName) const;
		int64_t AssignStreams (NodeContainer c, int64_t stream);
	private:
		Ptr<Application> InstallPriv (Ptr<Node> node) const;
		ObjectFactory m_factory; 
	};
} // namespace ns3

// myOnOffHelper.cc, change from on-off-helper.cc in ns3
namespace ns3 {
	myOnOffHelper::myOnOffHelper (std::string protocol, Address address)
	{
		m_factory.SetTypeId ("ns3::myOnOffApplication");
		m_factory.Set ("Protocol", StringValue (protocol));
		m_factory.Set ("Remote", AddressValue (address));
	}
	void myOnOffHelper::SetAttribute (std::string name, const AttributeValue &value)
	{
		m_factory.Set (name, value);
	}
	ApplicationContainer myOnOffHelper::Install (Ptr<Node> node) const
	{
		return ApplicationContainer (InstallPriv (node));
	}
	ApplicationContainer myOnOffHelper::Install (std::string nodeName) const
	{
		Ptr<Node> node = Names::Find<Node> (nodeName);
		return ApplicationContainer (InstallPriv (node));
	}
	ApplicationContainer myOnOffHelper::Install (NodeContainer c) const
	{
		ApplicationContainer apps;
		for (NodeContainer::Iterator i = c.Begin (); i != c.End (); ++i)
			apps.Add (InstallPriv (*i));
		return apps;
	}
	Ptr<Application> myOnOffHelper::InstallPriv (Ptr<Node> node) const
	{
		Ptr<Application> app = m_factory.Create<Application> ();
		node->AddApplication (app);
		return app;
	}
	int64_t myOnOffHelper::AssignStreams (NodeContainer c, int64_t stream)
	{
		int64_t currentStream = stream;
		Ptr<Node> node;
		for (NodeContainer::Iterator i = c.Begin (); i != c.End (); ++i)
		{
			node = (*i);
			for (uint32_t j = 0; j < node->GetNApplications (); j++)
			{
				Ptr<myOnOffApplication> onoff = DynamicCast<myOnOffApplication> (node->GetApplication (j));
				if (onoff)
				{
					currentStream += onoff->AssignStreams (currentStream);
				}
			}
		}
		return (currentStream - stream);
	}
	void myOnOffHelper::SetConstantRate (DataRate dataRate, uint32_t packetSize)
	{
		m_factory.Set ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1000]"));
		m_factory.Set ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
		m_factory.Set ("DataRate", DataRateValue (dataRate));
	 // m_factory.Set ("PacketSize", UintegerValue (packetSize));
		std::string packetSize_s = std::to_string (packetSize);
		std::string packetSize_str = "ns3::ConstantRandomVariable[Constant=" + packetSize_s + "]";
		m_factory.Set ("PacketSize", StringValue (packetSize_str));
		m_factory.Set ("Interval", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
	}
} // namespace ns3

// mySequentialRandomVariable.h, change from random-variable-stream.h in ns3
namespace ns3 {
	class RngStream;
	class mySequentialRandomVariable : public RandomVariableStream
	{
	public:
		static TypeId GetTypeId (void);
		mySequentialRandomVariable ();
		double GetMin (void) const;
		double GetMax (void) const;
		double GetIncrement (void) const; // change from ConstantRandomVariable
		uint32_t GetConsecutive (void) const;
		virtual double GetValue (void);
		virtual uint32_t GetInteger (void);

	private:
		double m_min;
		double m_max;
		double m_increment; // change from ConstantRandomVariable
		uint32_t m_consecutive;
		double m_current;
		uint32_t m_currentConsecutive;
		bool m_isCurrentSet;
		bool upflag;
		bool downflag;
	};
} // namespace ns3

// mySequentialRandomVariable.cc, change from random-variable-stream.cc in ns3
namespace ns3 {
	NS_OBJECT_ENSURE_REGISTERED (mySequentialRandomVariable);

	TypeId
	mySequentialRandomVariable::GetTypeId (void)
	{
		static TypeId tid = TypeId ("ns3::mySequentialRandomVariable")
			.SetParent<RandomVariableStream>()
			.SetGroupName ("Core")
			.AddConstructor<mySequentialRandomVariable> ()
			.AddAttribute ("Min", "The first value of the sequence.",
										 DoubleValue (0),
										 MakeDoubleAccessor (&mySequentialRandomVariable::m_min),
										 MakeDoubleChecker<double>())
			.AddAttribute ("Max", "One more than the last value of the sequence.",
										 DoubleValue (0),
										 MakeDoubleAccessor (&mySequentialRandomVariable::m_max),
										 MakeDoubleChecker<double>())
			.AddAttribute ("Increment", "The sequence random variable increment.",
										 DoubleValue (1),
										 MakeDoubleAccessor (&mySequentialRandomVariable::m_increment),
										 MakeDoubleChecker<double>()) // change from ConstantRandomVariable
			.AddAttribute ("Consecutive", "The number of times each member of the sequence is repeated.",
										 IntegerValue (1),
										 MakeIntegerAccessor (&mySequentialRandomVariable::m_consecutive),
										 MakeIntegerChecker<uint32_t>());
		return tid;
	}
	mySequentialRandomVariable::mySequentialRandomVariable ()
		:
			m_current            (0),
			m_currentConsecutive (0),
			m_isCurrentSet       (false),
			upflag               (true)
	{
		// m_min, m_max, m_increment, and m_consecutive are initialized
		// after constructor by attributes.
		NS_LOG_FUNCTION (this);
	}

	double
	mySequentialRandomVariable::GetMin (void) const
	{
		NS_LOG_FUNCTION (this);
		return m_min;
	}

	double
	mySequentialRandomVariable::GetMax (void) const
	{
		NS_LOG_FUNCTION (this);
		return m_max;
	}

	double
	mySequentialRandomVariable::GetIncrement (void) const
	{
		NS_LOG_FUNCTION (this);
		return m_increment;
	} // change from ConstantRandomVariable

	uint32_t
	mySequentialRandomVariable::GetConsecutive (void) const
	{
		NS_LOG_FUNCTION (this);
		return m_consecutive;
	}

	double
	mySequentialRandomVariable::GetValue (void)
	{
		// Set the current sequence value if it hasn't been set.
		NS_LOG_FUNCTION (this);
		if (!m_isCurrentSet)
			{
				// Start the sequence at its minimium value.
				m_current = m_min;
				m_isCurrentSet = true;
			}

		// Return a sequential series of values
		double r = m_current;
		if (++m_currentConsecutive == m_consecutive)
			{ // Time to advance to next
				m_currentConsecutive = 0;

				if (m_increment >= 0)
				{
					if (m_current >= m_max)
					{
						downflag = true;
						upflag = false;
					}
					if (m_current <= m_min)
					{
						upflag = true;
						downflag = false;
					}
				}
				else // For m_max is small, m_min is big, and m_increment is negative value (-1).
				{
					if (m_current <= m_max)
					{
						upflag = false;
						downflag = true;
					}
					if (m_current >= m_min)
					{
						downflag = false;
						upflag = true;
					}
				}
				if (upflag == true)
				{
					m_current += m_increment; 
				}
				if (downflag == true)
				{
					m_current -= m_increment;
				}
				
			}
		return r;
	}

	uint32_t
	mySequentialRandomVariable::GetInteger (void)
	{
		NS_LOG_FUNCTION (this);
		return (uint32_t)GetValue ();
	}
}


/*
// adaptiveAgg.h, from my own
namespace ns3 {
	class adaptiveAgg
	{
	public:
		adaptiveAgg();
		virtual ~adaptiveAgg();
		void Set_rcts_thr (std::string threshold);
	};
}

//adaptiveAgg.cc, from my own
namespace ns3{
	// NS_LOG_COMPONENT_DEFINE ("adaptiveAgg");
	// NS_OBJECT_ENSURE_REGISTERED (adaptiveAgg);
	adaptiveAgg::adaptiveAgg ()
	{ NS_LOG_FUNCTION (this); }
	adaptiveAgg::~adaptiveAgg ()
	{ NS_LOG_FUNCTION (this); }

	void adaptiveAgg::Set_rcts_thr (std::string threshold)
	{
		Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue (threshold)); // so as to force RTS/CTS for only data frames
	}
}
*/
// Default Network Topology
//
// 192.168.25.2   25.1+35.2     35.1+45.2     45.1+11.2     192.168.11.1
//      STA        AP+STA        AP+STA        AP+STA           AP
//       *           *             *             *              *
//   y^  |           |             |             |              | 
//    |  U----------R0------------R1------------R2--------------S
//    | 0.0        5.0           10.0          15.0           20.0
//    -------------------------------------------------------------->x

double nSamplingPeriod = 0.01;   // 抽样间隔，根据总的Simulation时间做相应的调整
using namespace ns3;
NS_LOG_COMPONENT_DEFINE ("WSOL_Base");
/*
 * Calculate Throughput using Flowmonitor
 * 每个探针(probe)会根据四点来对包进行分类
 * -- when packet is `sent`;
 * -- when packet is `forwarded`;
 * -- when packet is `received`;
 * -- when packet is `dropped`;
 * 由于包是在IP层进行track的，所以任何的四层(TCP)重传的包，都会被认为是一个新的包
 */
void ThroughputMonitor (FlowMonitorHelper* fmhelper, Ptr<FlowMonitor> monitor, Gnuplot2dDataset dataset)
{
	double throu   = 0.0;
	monitor->CheckForLostPackets ();
	std::map<FlowId, FlowMonitor::FlowStats> flowStats = monitor->GetFlowStats ();
	/* since fmhelper is a pointer, we should use it as a pointer.
	 * `fmhelper->GetClassifier ()` instead of `fmhelper.GetClassifier ()`
	 */
	Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (fmhelper->GetClassifier ());
	for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = flowStats.begin (); i != flowStats.end (); ++i)
	{
	/* `Ipv4FlowClassifier`
	 * Classifies packets by looking at their IP and TCP/UDP headers. 
	 * FiveTuple五元组是：(source-ip, destination-ip, protocol, source-port, destination-port)
	 * 每个flow是根据包的五元组(协议，源IP/端口，目的IP/端口)来区分的 */
	Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
	// `192.168.0.11`是client(Node #14)的IP
	// `192.168.0.7` 是client(Node #10)的IP
		if ((t.sourceAddress=="192.168.25.2" && t.destinationAddress == "192.168.11.1"))
		{
				// UDP_PROT_NUMBER = 17
			if (17 == unsigned(t.protocol))
			{
				//bytes*8=bits, bits/s=bps, 1bps/1024/1024=Mbps, 1bps=10e6 bpus, 10e6/1024/1024=0.9537 => throu (Mbps)
				throu   = i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetMicroSeconds() - i->second.timeFirstTxPacket.GetMicroSeconds()) * 0.9537 ;
				dataset.Add  (Simulator::Now().GetSeconds(), throu);
			}
			else
			{
				std::cout << "This is not UDP traffic" << std::endl;
			}
		}
	}
	/* check throughput every nSamplingPeriod second(每隔nSamplingPeriod调用1次Simulation)
	 * 表示每隔nSamplingPeriod时间*/
	Simulator::Schedule (Seconds(nSamplingPeriod), &ThroughputMonitor, fmhelper, monitor, dataset);
}

void DelayMonitor (FlowMonitorHelper* fmhelper, Ptr<FlowMonitor> monitor, Gnuplot2dDataset dataset1)
{
	double delay   = 0.0;
	double rxpackets = 0.0;
	double mean_delay = 0.0;
	monitor->CheckForLostPackets ();
	std::map<FlowId, FlowMonitor::FlowStats> flowStats = monitor->GetFlowStats ();
	Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (fmhelper->GetClassifier ());
	for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = flowStats.begin (); i != flowStats.end (); ++i)
		{
			Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
			if ((t.sourceAddress=="192.168.25.2" && t.destinationAddress == "192.168.11.1"))
				{
					// UDP_PROT_NUMBER = 17
					if (17 == unsigned(t.protocol))
					{
						//lastDelay: The last measured delay of a packet.
						delay   = i->second.delaySum.GetMilliSeconds ();  //MilliSeconds, ms
						rxpackets = i->second.rxPackets;
						mean_delay = delay / rxpackets;
						dataset1.Add (Simulator::Now().GetSeconds(), mean_delay);
					}
					else
					{
						std::cout << "This is not UDP traffic" << std::endl;
					}
				}
		}
	Simulator::Schedule (Seconds(nSamplingPeriod), &DelayMonitor, fmhelper, monitor, dataset1);
}

void LostPacketsMonitor (FlowMonitorHelper* fmhelper, Ptr<FlowMonitor> monitor, Gnuplot2dDataset dataset2)
{
	double packets = 0.0;
	double rxpackets = 0.0;
	double packet_loss_ratio = 0.0;
	monitor->CheckForLostPackets ();
	std::map<FlowId, FlowMonitor::FlowStats> flowStats = monitor->GetFlowStats ();
	Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (fmhelper->GetClassifier ());
	for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = flowStats.begin (); i != flowStats.end (); ++i)
		{
			Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
			if ((t.sourceAddress=="192.168.25.2" && t.destinationAddress == "192.168.11.1"))
				{
					// UDP_PROT_NUMBER = 17
					if (17 == unsigned(t.protocol))
					{
						packets = i->second.lostPackets;
						rxpackets = i -> second.rxPackets;
						packet_loss_ratio = packets / (rxpackets + packets);
						dataset2.Add (Simulator::Now().GetSeconds(), packet_loss_ratio);
					}
					else
					{
						std::cout << "This is not UDP traffic" << std::endl;
					}
				}
		}
	Simulator::Schedule (Seconds(nSamplingPeriod), &LostPacketsMonitor, fmhelper, monitor, dataset2);
}

void JitterMonitor (FlowMonitorHelper* fmhelper, Ptr<FlowMonitor> monitor, Gnuplot2dDataset dataset3)
{
	double jitter  = 0.0;
	double rxpackets = 0.0;
	double mean_jitter = 0.0;
	monitor->CheckForLostPackets ();
	std::map<FlowId, FlowMonitor::FlowStats> flowStats = monitor->GetFlowStats ();
	Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (fmhelper->GetClassifier ());
	for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = flowStats.begin (); i != flowStats.end (); ++i)
		{
			Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
			if ((t.sourceAddress=="192.168.25.2" && t.destinationAddress == "192.168.11.1"))
				{
					// UDP_PROT_NUMBER = 17
					if (17 == unsigned(t.protocol))
					{
						jitter  = i->second.jitterSum.GetMilliSeconds ();
						rxpackets = i -> second.rxPackets;
						mean_jitter = jitter / (rxpackets - 1);
						dataset3.Add (Simulator::Now().GetSeconds(), mean_jitter);
					}
					else
					{
						std::cout << "This is not UDP traffic" << std::endl;
					}
				}
		}
	Simulator::Schedule (Seconds(nSamplingPeriod), &JitterMonitor, fmhelper, monitor, dataset3);
}

void PacketSinkTrace (Ptr<const Packet> packet, const Address &ad)
{
	std::cout << " RX packet size: " << packet->GetSize () << std::endl;
}
void Set_rcts_thr (uint32_t threshold)
{
	Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/RemoteStationManager/RtsCtsThreshold", UintegerValue (threshold));
}
void Set_amsdusize (uint16_t size)
{
	Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/$ns3::RegularWifiMac/BE_MaxAmsduSize", UintegerValue (size));
}
void Set_ampdusize (uint32_t size)
{
	Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/$ns3::RegularWifiMac/BE_MaxAmpduSize", UintegerValue (size));
}
static void
CourseChangeCallback (std::string path, Ptr<const MobilityModel> model)
{
  Vector position = model->GetPosition ();
  std::cout << "CourseChange " << path << " x=" << position.x << ", y=" << position.y << ", z=" << position.z << std::endl;
}

int main (int argc, char *argv[])
{
	bool verbose = true;
	bool tracing = false;
	std::string rcts_thr = "655350"; // Default 655350 off
	std::string interval = "0.0002";
	std::string pldsize = "1472";
	uint32_t ampdusize = 65535; // 65535 byte ht default
	uint32_t amsdusize = 0; // 7935 byte ht default

	CommandLine cmd;
	cmd.AddValue ("verbose", "Tell applications to log if true", verbose);
	cmd.AddValue ("tracing", "Enable pcap tracing", tracing);
	cmd.AddValue ("rcts_thr", "RTS/CTS Threshold", rcts_thr);
	cmd.AddValue ("interval", "Tell Client the send Interval", interval);
	cmd.AddValue ("pldsize", "Payload Size of Application Layer", pldsize);
	cmd.AddValue ("ampdusize", "A-MPDU Limit Size", ampdusize);
	cmd.AddValue ("amsdusize", "A-MSDU Limit Size", amsdusize);

	cmd.Parse (argc,argv);

	if (verbose)
	{
		LogComponentEnable("myOnOffApplication",LOG_LEVEL_INFO);
		LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
		LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);
	}

//-----------------------Nodes Define-------------------------------------
	Ptr<Node> nU = CreateObject<Node> ();		// Node0
	Ptr<Node> nR0 = CreateObject<Node> ();	// Node1
	Ptr<Node> nR1 = CreateObject<Node> ();	// Node2
	Ptr<Node> nR2 = CreateObject<Node> ();  // Node3
	Ptr<Node> nS = CreateObject<Node> ();		// Node4
	NodeContainer nodes = NodeContainer(nU, nR0, nR1, nR2, nS);
	// int numberOfnodes = 5; // remeber change it according to the number of nodes.
	// Sta-AP links
	NodeContainer nSnR2 = NodeContainer (nS, nR2);
	NodeContainer nR2nR1 = NodeContainer (nR2, nR1);
	NodeContainer nR1nR0 = NodeContainer (nR1, nR0);
	NodeContainer nR0nU = NodeContainer (nR0, nU);
	// Sta and AP nodes
	// NodeContainer nSTA = NodeContainer (nU, nR0, nR1);
	// NodeContainer nAP = NodeContainer (nR0, nR1, nS);

//-----------------------PHY Define---------------------------------------
	int mcs = 9; // Max MCS=9
	uint16_t channelWidth = 80;

	YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
	YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
	phy.SetChannel (channel.Create ());

	Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/VhtConfiguration/ShortGuardIntervalSupported", BooleanValue (true));
	std::cout << "mcs: " << mcs << "\tChannelWidth: " << channelWidth << " MHz" << std::endl;

//--------------------MAC and Device Define-------------------------------
	Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue (rcts_thr));
	// Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/$ns3::RegularWifiMac/Txop/Queue/MaxQueueSize", QueueSizeValue (QueueSize ("100p")));
	// Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/$ns3::RegularWifiMac/Txop/Queue/MaxDelay", TimeValue (MilliSeconds (100)));

	WifiHelper wifi;
	// wifi.SetRemoteStationManager ("ns3::AarfWifiManager");
	wifi.SetStandard (WIFI_STANDARD_80211ac);

	std::ostringstream oss;
	oss << "VhtMcs" << mcs;
	wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager","DataMode", StringValue (oss.str ()),
																"ControlMode", StringValue (oss.str ()));
	WifiMacHelper apmac;
	Ssid ssid_11 = Ssid ("openwrt11");
	apmac.SetType ("ns3::ApWifiMac",
				"Ssid", SsidValue (ssid_11),
				"QosSupported", BooleanValue (true),
				"BE_MaxAmsduSize", UintegerValue (amsdusize),
				"BE_MaxAmpduSize", UintegerValue (ampdusize));
	WifiMacHelper stamac;
	stamac.SetType ("ns3::StaWifiMac",
				"Ssid", SsidValue (ssid_11),
				"QosSupported", BooleanValue (true),
				"ActiveProbing", BooleanValue (false), 
				"BE_MaxAmsduSize", UintegerValue (amsdusize),
				"BE_MaxAmpduSize", UintegerValue (ampdusize));
	NetDeviceContainer dSdR2;
	dSdR2 = wifi.Install (phy, apmac, nS);
	dSdR2.Add (wifi.Install (phy, stamac, nR2));
	//Modify EDCA configuration (TXOP limit) for AC_BE
	Ptr<NetDevice> dev = dSdR2.Get (0);
	Ptr<WifiNetDevice> wifi_dev = DynamicCast<WifiNetDevice> (dev);
	Ptr<WifiMac> wifi_mac = wifi_dev->GetMac ();
	PointerValue ptr;
	Ptr<QosTxop> edca;
	wifi_mac->GetAttribute ("BE_Txop", ptr); //!!!
	edca = ptr.Get<QosTxop> ();
	edca->SetTxopLimit (MicroSeconds (0)); //!!!

	Ssid ssid_45 = Ssid ("openwrt45");
	apmac.SetType ("ns3::ApWifiMac",
				"Ssid", SsidValue (ssid_45),
				"QosSupported", BooleanValue (true),
				"BE_MaxAmsduSize", UintegerValue (amsdusize),
				"BE_MaxAmpduSize", UintegerValue (ampdusize));
	stamac.SetType ("ns3::StaWifiMac",
				"Ssid", SsidValue (ssid_45),
				"QosSupported", BooleanValue (true),
				"ActiveProbing", BooleanValue (false), 
				"BE_MaxAmsduSize", UintegerValue (amsdusize),
				"BE_MaxAmpduSize", UintegerValue (ampdusize));
	NetDeviceContainer dR2dR1;
	dR2dR1 = wifi.Install (phy, apmac, nR2);
	dR2dR1.Add (wifi.Install (phy, stamac, nR1));
	//Modify EDCA configuration (TXOP limit) for AC_BE
	dev = dR2dR1.Get (0);
	wifi_dev = DynamicCast<WifiNetDevice> (dev);
	wifi_mac = wifi_dev->GetMac ();
	wifi_mac->GetAttribute ("BE_Txop", ptr);
	edca = ptr.Get<QosTxop> ();
	edca->SetTxopLimit (MicroSeconds (0));

	Ssid ssid_35 = Ssid ("openwrt35");
	apmac.SetType ("ns3::ApWifiMac",
				"Ssid", SsidValue (ssid_35),
				"QosSupported", BooleanValue (true),
				"BE_MaxAmsduSize", UintegerValue (amsdusize),
				"BE_MaxAmpduSize", UintegerValue (ampdusize));
	stamac.SetType ("ns3::StaWifiMac",
				"Ssid", SsidValue (ssid_35),
				"QosSupported", BooleanValue (true),
				"ActiveProbing", BooleanValue (false),
				"BE_MaxAmsduSize", UintegerValue (amsdusize),
				"BE_MaxAmpduSize", UintegerValue (ampdusize));
	NetDeviceContainer dR1dR0;
	dR1dR0 = wifi.Install (phy, apmac, nR1);
	dR1dR0.Add (wifi.Install (phy, stamac, nR0));
	//Modify EDCA configuration (TXOP limit) for AC_BE
	dev = dR1dR0.Get (0);
	wifi_dev = DynamicCast<WifiNetDevice> (dev);
	wifi_mac = wifi_dev->GetMac ();
	wifi_mac->GetAttribute ("BE_Txop", ptr);
	edca = ptr.Get<QosTxop> ();
	edca->SetTxopLimit (MicroSeconds (0));


	Ssid ssid_25 = Ssid ("openwrt25");
	apmac.SetType ("ns3::ApWifiMac",
				"Ssid", SsidValue (ssid_25),
				"QosSupported", BooleanValue (true),
				"BE_MaxAmsduSize", UintegerValue (amsdusize),
				"BE_MaxAmpduSize", UintegerValue (ampdusize));
	stamac.SetType ("ns3::StaWifiMac",
				"Ssid", SsidValue (ssid_25),
				"QosSupported", BooleanValue (true),
				"ActiveProbing", BooleanValue (false),
				"BE_MaxAmsduSize", UintegerValue (amsdusize),
				"BE_MaxAmpduSize", UintegerValue (ampdusize));
	NetDeviceContainer dR0dU;
	dR0dU = wifi.Install (phy, apmac, nR0);
	dR0dU.Add (wifi.Install (phy, stamac, nU));
	//Modify EDCA configuration (TXOP limit) for AC_BE
	dev = dR0dU.Get (0);
	wifi_dev = DynamicCast<WifiNetDevice> (dev);
	wifi_mac = wifi_dev->GetMac ();
	wifi_mac->GetAttribute ("BE_Txop", ptr);
	edca = ptr.Get<QosTxop> ();
	edca->SetTxopLimit (MicroSeconds (0));
	
	Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/ChannelWidth", UintegerValue (channelWidth));

	InternetStackHelper stack;
	stack.Install (nodes);

//-----------------Mobility model(Must for wifi)--------------------------
	MobilityHelper mobility;
	Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
	positionAlloc->Add (Vector (0.0, 0.0, 0.0));
	positionAlloc->Add (Vector (7.0, 0.0, 0.0));
	positionAlloc->Add (Vector (14.0, 0.0, 0.0));
	positionAlloc->Add (Vector (21.0, 0.0, 0.0));
	positionAlloc->Add (Vector (28.0, 0.0, 0.0));
	mobility.SetPositionAllocator (positionAlloc);
	mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
	mobility.Install (nodes);
	
	Config::Connect ("/NodeList/*/$ns3::MobilityModel/CourseChange", MakeCallback (&CourseChangeCallback));
//-------------------Interface and IP Define------------------------------
	Ipv4AddressHelper address;
	address.SetBase ("192.168.11.0", "255.255.255.0");
	Ipv4InterfaceContainer iSiR2 = address.Assign (dSdR2);
	address.SetBase ("192.168.45.0", "255.255.255.0");
	Ipv4InterfaceContainer iR2iR1 = address.Assign (dR2dR1);
	address.SetBase ("192.168.35.0", "255.255.255.0");
	Ipv4InterfaceContainer iR1iR0 = address.Assign (dR1dR0);
	address.SetBase ("192.168.25.0", "255.255.255.0");
	Ipv4InterfaceContainer iR0iU = address.Assign (dR0dU);

	Ptr<Ipv4> ipv4S = nS->GetObject<Ipv4> ();
	Ptr<Ipv4> ipv4R2 = nR2->GetObject<Ipv4> ();
	Ptr<Ipv4> ipv4R1 = nR1->GetObject<Ipv4> ();
	Ptr<Ipv4> ipv4R0 = nR0->GetObject<Ipv4> ();
	Ptr<Ipv4> ipv4U = nU->GetObject<Ipv4> ();

//----------------------Route Table Define--------------------------------
	Ipv4StaticRoutingHelper ipv4RoutingHelper;
	Ptr<Ipv4StaticRouting> staticRoutingS = ipv4RoutingHelper.GetStaticRouting (ipv4S);
	Ptr<Ipv4StaticRouting> staticRoutingR2 = ipv4RoutingHelper.GetStaticRouting (ipv4R2);
	Ptr<Ipv4StaticRouting> staticRoutingR1 = ipv4RoutingHelper.GetStaticRouting (ipv4R1);
	Ptr<Ipv4StaticRouting> staticRoutingR0 = ipv4RoutingHelper.GetStaticRouting (ipv4R0);
	Ptr<Ipv4StaticRouting> staticRoutingU = ipv4RoutingHelper.GetStaticRouting (ipv4U);

	staticRoutingS->AddNetworkRouteTo (Ipv4Address ("192.168.25.0"), Ipv4Mask ("255.255.255.0"), Ipv4Address ("192.168.11.2"), 1);
	staticRoutingS->AddNetworkRouteTo (Ipv4Address ("192.168.35.0"), Ipv4Mask ("255.255.255.0"), Ipv4Address ("192.168.11.2"), 1);
	staticRoutingS->AddNetworkRouteTo (Ipv4Address ("192.168.45.0"), Ipv4Mask ("255.255.255.0"), Ipv4Address ("192.168.11.2"), 1);

	staticRoutingR2->AddNetworkRouteTo (Ipv4Address ("192.168.25.0"), Ipv4Mask ("255.255.255.0"), Ipv4Address ("192.168.45.2"), 2);
	staticRoutingR2->AddNetworkRouteTo (Ipv4Address ("192.168.35.0"), Ipv4Mask ("255.255.255.0"), Ipv4Address ("192.168.45.2"), 2);

	staticRoutingR1->AddNetworkRouteTo (Ipv4Address ("192.168.25.0"), Ipv4Mask ("255.255.255.0"), Ipv4Address ("192.168.35.2"), 2);
	staticRoutingR1->AddNetworkRouteTo (Ipv4Address ("192.168.11.0"), Ipv4Mask ("255.255.255.0"), Ipv4Address ("192.168.45.1"), 1);
	
	staticRoutingR0->AddNetworkRouteTo (Ipv4Address ("192.168.45.0"), Ipv4Mask ("255.255.255.0"), Ipv4Address ("192.168.35.1"), 1);
	staticRoutingR0->AddNetworkRouteTo (Ipv4Address ("192.168.11.0"), Ipv4Mask ("255.255.255.0"), Ipv4Address ("192.168.35.1"), 1);

	staticRoutingU->AddNetworkRouteTo (Ipv4Address ("192.168.11.0"), Ipv4Mask ("255.255.255.0"), Ipv4Address ("192.168.25.1"), 1);
	staticRoutingU->AddNetworkRouteTo (Ipv4Address ("192.168.35.0"), Ipv4Mask ("255.255.255.0"), Ipv4Address ("192.168.25.1"), 1);
	staticRoutingU->AddNetworkRouteTo (Ipv4Address ("192.168.45.0"), Ipv4Mask ("255.255.255.0"), Ipv4Address ("192.168.25.1"), 1);

//-----------------------APP Define---------------------------------------
	uint16_t server_port = 9;
	Address localAddress (InetSocketAddress (Ipv4Address::GetAny (), server_port));
	PacketSinkHelper packetSinkHelper ("ns3::UdpSocketFactory", localAddress);
	ApplicationContainer serverApps = packetSinkHelper.Install (nS);
	serverApps.Start (Seconds (0.1));
	serverApps.Stop (Seconds (11.0));

	myOnOffHelper onoff ("ns3::UdpSocketFactory", Ipv4Address::GetAny ());
	onoff.SetAttribute ("OnTime",  StringValue ("ns3::ConstantRandomVariable[Constant=40]"));
	onoff.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
	onoff.SetAttribute ("MaxBytes", UintegerValue(0)); // 0 means infinite

	std::string Rng_string1 = "ns3::ConstantRandomVariable[Constant=" + pldsize + "]";
	onoff.SetAttribute ("PacketSize", StringValue (Rng_string1));
	// onoff.SetAttribute ("PacketSize", StringValue ("ns3::UniformRandomVariable[Min=1000|Max=1472]"));
	onoff.SetAttribute ("PacketSize", StringValue ("ns3::ConstantRandomVariable[Constant=1472]"));
	/*
	Ptr<mySequentialRandomVariable> x = CreateObject<mySequentialRandomVariable> ();
	x->SetAttribute ("Min", DoubleValue (200)); // must start from Min.
	x->SetAttribute ("Max", DoubleValue (1472));
	x->SetAttribute ("Consecutive", IntegerValue (32825));
	x->SetAttribute ("Increment", DoubleValue (1272));
	onoff.SetAttribute ("PacketSize", PointerValue (x));
	*/

	std::string Rng_string2 = "ns3::ConstantRandomVariable[Constant=" + interval + "]";
	onoff.SetAttribute ("Interval", StringValue (Rng_string2));
	onoff.SetAttribute ("Interval", StringValue ("ns3::ConstantRandomVariable[Constant=0.00017]"));
	/*
	Ptr<mySequentialRandomVariable> y = CreateObject<mySequentialRandomVariable> ();
	y->SetAttribute ("Min", DoubleValue (0.00015)); // must start from Min.
	y->SetAttribute ("Max", DoubleValue (0.00045));
	y->SetAttribute ("Consecutive", IntegerValue (546));
	y->SetAttribute ("Increment", DoubleValue (0.00001));
	onoff.SetAttribute ("Interval",  PointerValue (y));
	*/
	AddressValue remoteAddress (InetSocketAddress (iSiR2.GetAddress (0), 9));
	onoff.SetAttribute ("Remote", remoteAddress);
	ApplicationContainer clientApp = onoff.Install (nU);
	clientApp.Start (Seconds (0.2));
	clientApp.Stop (Seconds (10.0));

//-----------------------Adaptive Function--------------------------------------

		// Simulator::Schedule (Seconds (14.0), &Set_rcts_thr, 655350);
		// Simulator::Schedule (Seconds (7.0), &Set_amsdusize, 5000);
		// Simulator::Schedule (Seconds (0.01), &Set_amsdusize, 6200);
		// Simulator::Schedule (Seconds (16.0), &Set_amsdusize, 0);
		// Simulator::Schedule (Seconds (24.0), &Set_amsdusize, 6200);

		// Simulator::Schedule (Seconds (16.0), &Set_ampdusize, 38000);
		// Simulator::Schedule (Seconds (20.5), &Set_ampdusize, 65535);


//-----------------------Data Analyse-------------------------------------------
	FlowMonitorHelper flowmon;
	Ptr<FlowMonitor> monitor = flowmon.InstallAll();

	Config::ConnectWithoutContext ("/NodeList/*/ApplicationList/*/$ns3::PacketSink/Rx", MakeCallback (&PacketSinkTrace));
	
	Simulator::Stop (Seconds (12.0));

// GNUplot parameters
	std::string base = "WSOL_Base";
	//Throughput
	std::string throu = base + "ThroughputVSTime";
	std::string graphicsFileName        = throu + ".png";
	std::string plotFileName            = throu + ".plt";
	std::string plotTitle               = "Throughput vs Time";
	std::string dataTitle               = "Throughput";
	Gnuplot gnuplot (graphicsFileName);
	gnuplot.SetTitle (plotTitle);
	gnuplot.SetTerminal ("png");
	gnuplot.SetLegend ("Time (s)", "Throughput (Mbps)");
	Gnuplot2dDataset dataset;
	dataset.SetTitle (dataTitle);
	dataset.SetStyle (Gnuplot2dDataset::LINES_POINTS);
	//Delay
	std::string delay = base + "DelayVSTime";
	std::string graphicsFileName1        = delay + ".png";
	std::string plotFileName1            = delay + ".plt";
	std::string plotTitle1               = "Mean Delay vs Time";
	std::string dataTitle1               = "Mean Delay";
	Gnuplot gnuplot1 (graphicsFileName1);
	gnuplot1.SetTitle (plotTitle1);
	gnuplot1.SetTerminal ("png");
	gnuplot1.SetLegend ("Time (s)", "Mean Delay (ms)");
	Gnuplot2dDataset dataset1;
	dataset1.SetTitle (dataTitle1);
	dataset1.SetStyle (Gnuplot2dDataset::LINES_POINTS);
	//LostPackets
	std::string lost = base + "LostPacketsVSTime";
	std::string graphicsFileName2        = lost + ".png";
	std::string plotFileName2            = lost + ".plt";
	std::string plotTitle2               = "LostPackets Ratio vs Time";
	std::string dataTitle2               = "LostPackets Ratio";
	Gnuplot gnuplot2 (graphicsFileName2);
	gnuplot2.SetTitle (plotTitle2);
	gnuplot2.SetTerminal ("png");
	gnuplot2.SetLegend ("Time (s)", "LostPackets Ratio (%)");
	Gnuplot2dDataset dataset2;
	dataset2.SetTitle (dataTitle2);
	dataset2.SetStyle (Gnuplot2dDataset::LINES_POINTS);
	//Jitter
	std::string jitter = base + "JitterVSTime";
	std::string graphicsFileName3        = jitter + ".png";
	std::string plotFileName3            = jitter + ".plt";
	std::string plotTitle3               = "Jitter vs Time";
	std::string dataTitle3               = "Jitter";
	Gnuplot gnuplot3 (graphicsFileName3);
	gnuplot3.SetTitle (plotTitle3);
	gnuplot3.SetTerminal ("png");
	gnuplot3.SetLegend ("Time (s)", "Jitter (ms)");
	Gnuplot2dDataset dataset3;
	dataset3.SetTitle (dataTitle3);
	dataset3.SetStyle (Gnuplot2dDataset::LINES_POINTS);

	// 测吞吐量, 延时, 丢包, 抖动, 最后打印出这些参数
	ThroughputMonitor (&flowmon, monitor, dataset);
	DelayMonitor      (&flowmon, monitor, dataset1);
	LostPacketsMonitor(&flowmon, monitor, dataset2);
	JitterMonitor     (&flowmon, monitor, dataset3);
/*-----------------------------------------------------*/

	if (tracing == true)
	{
		// pointToPoint.EnablePcapAll ("WSOL");
		phy.EnablePcap ("WSOL_Base", dSdR2.Get (0));
		phy.EnablePcap ("WSOL_Base", dSdR2.Get (1)); // WSOL_Base-3-0
		phy.EnablePcap ("WSOL_Base", dR2dR1.Get (0)); // WSOL_Base-3-1
		// wifiphy.EnablePcap ("WSOL_Base", dSdR2.Get (0));
		// phy.EnablePcapAll ("WSOL_Base");	//<name>-<node>-<device>.pcap, <node>是在node create时确定的。
		// wifi.EnablePcap ("WSOL_Base", dSdR2.Get (0), true);
	}
	// NS_LOG_INFO ("------------Running Simulation.------------");

	Simulator::Run ();

	//Throughput
	gnuplot.AddDataset (dataset);
	std::ofstream plotFile (plotFileName.c_str());
	gnuplot.GenerateOutput (plotFile);
	plotFile.close ();
	//Delay
	gnuplot1.AddDataset (dataset1);
	std::ofstream plotFile1 (plotFileName1.c_str());
	gnuplot1.GenerateOutput (plotFile1);
	plotFile1.close ();
	//LostPackets
	gnuplot2.AddDataset (dataset2);
	std::ofstream plotFile2 (plotFileName2.c_str());
	gnuplot2.GenerateOutput (plotFile2);
	plotFile2.close ();
	//Jitter
	gnuplot3.AddDataset (dataset3);
	std::ofstream plotFile3 (plotFileName3.c_str());
	gnuplot3.GenerateOutput (plotFile3);
	plotFile3.close ();

	monitor->SerializeToXmlFile("WSOL_Base.xml", true, true);
	/* the SerializeToXmlFile () function 2nd and 3rd parameters 
	 * are used respectively to activate/deactivate the histograms and the per-probe detailed stats.
	 */

	Simulator::Destroy ();
	return 0;
}
