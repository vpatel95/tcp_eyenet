/*
 * =====================================================================================
 *
 *       Filename:  tcp-eyenet.cc
 *
 *    Description:  Network Topology for TCP Eyenet
 *
 *
 *                          node 0                 node 1
 *                    +----------------+    +----------------+
 *                    |    ns-3 TCP    |    |    ns-3 TCP    |
 *                    +----------------+    +----------------+
 *                    |    10.1.1.1    |    |    10.1.1.2    |
 *                    +----------------+    +----------------+
 *                    | point-to-point |    | point-to-point |
 *                    +----------------+    +----------------+
 *                            |                     |
 *                            +---------------------+
 *                                 5 Mbps, 2 ms
 *
 *
 *        Version:  1.0
 *       Revision:  none
 *       Compiler:  g++
 *
 *         Author:  Ved Patel, vmp5265@psu.edu
 *
 * =====================================================================================
 */

#include <iostream>
#include <string>
#include <fstream>

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("SixthScriptExample");

class TCPEyenetNetwork : public Application {
    public:
        TCPEyenetNetwork();
        virtual ~TCPEyenetNetwork();

        static TypeId GetTypeId(void);
        void Setup(Ptr<Socket> socket, Address remoteAddr, uint32_t pktSize,
                   uint32_t nPkts, DataRate dataRate);

    private:
        virtual void StartApplication(void);
        virtual void StopApplication(void);

        void ScheduleTx(void);
        void SendPackets(void);

        bool            m_running;
        Ptr<Socket>     m_socket;
        Address         m_peer;
        uint32_t        m_pktSize,
                        m_nPkts,
                        m_pktSent;
        DataRate        m_dataRate;
        EventId         m_sendEvent;
};

TCPEyenetNetwork::TCPEyenetNetwork() :
    m_running(false),
    m_socket(0),
    m_peer(),
    m_pktSize(0),
    m_nPkts(0),
    m_pktSent(0),
    m_dataRate(0),
    m_sendEvent(){
}

TCPEyenetNetwork::~TCPEyenetNetwork() {
    m_socket = 0;
}

TypeId TCPEyenetNetwork::GetTypeId(void) {
    static TypeId tid = TypeId("TCPEyenetNetwork").
                            SetParent<Application>().
                            SetGroupName("TCPEyenet").
                            AddConstructor<TCPEyenetNetwork>();

    return tid;
}

void TCPEyenetNetwork::Setup(Ptr<Socket> sk, Address addr,
                        uint32_t pktSize, uint32_t nPkts,
                        DataRate dataRate) {
    m_socket = sk;
    m_peer = addr;
    m_pktSize = pktSize;
    m_nPkts = nPkts;
    m_dataRate = dataRate;
}

void TCPEyenetNetwork::StartApplication(void) {
    m_running = true;
    m_pktSent = 0;
    m_socket->Bind();
    m_socket->Connect(m_peer);
    SendPackets();
}

void TCPEyenetNetwork::SendPackets(void) {
    Ptr<Packet> pkt = Create<Packet>(m_pktSize);
    m_socket->Send(pkt);

    if (++m_pktSent < m_nPkts) {
        ScheduleTx();
    }
}

void TCPEyenetNetwork::ScheduleTx(void) {
    if (m_running) {
        Time tNext (Seconds(m_pktSize * 8 / static_cast<double>(m_dataRate.GetBitRate())));
        m_sendEvent = Simulator::Schedule(tNext, &TCPEyenetNetwork::SendPackets, this);
    }
}

void TCPEyenetNetwork::StopApplication (void) {
    m_running = false;

    if (m_sendEvent.IsRunning()) {
        Simulator::Cancel(m_sendEvent);
    }

    if (m_socket) {
        m_socket->Close();
    }
}

static inline void CwndChange(Ptr<OutputStreamWrapper> stream, uint32_t prevCwnd, uint32_t currCwnd) {
    NS_LOG_UNCOND(Simulator::Now().GetSeconds() << "," << prevCwnd << "," << currCwnd);
    *stream->GetStream() << Simulator::Now().GetSeconds() << "," << prevCwnd << "," << currCwnd << std::endl;
}

int32_t main(int32_t argc, char *argv[]) {

    uint16_t        port = 8080;
    uint32_t        stopTime = 60,
                    //startTime = 0,
                    pktSize = 1040,
                    nPkts = 10000000;
    std::string     socketFactory = "ns3::TcpSocketFactory",
                    dataRate = "1Mbps";

    CommandLine cmd (__FILE__);
    cmd.Parse(argc, argv);

    NodeContainer container;
    container.Create(3);

    // Segregate connection between nodes into seperate containers

    // Sender--Router connection
    NodeContainer senderRouter = NodeContainer(container.Get(0), container.Get(1));
    // Router--Receiver connection
    NodeContainer routerReceiver = NodeContainer(container.Get(1), container.Get(2));

    // Install Internet stack on all the nodes.
    InternetStackHelper inetStack;
    inetStack.Install(container);

    PointToPointHelper link;

    // Set link characteristics for senderRouter
    link.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    link.SetChannelAttribute("Delay", StringValue("8ms"));

    // Create the link and set router error model
    NetDeviceContainer senderRouterLink = link.Install(senderRouter);

    Ptr<RateErrorModel> routerError = CreateObject<RateErrorModel>();
    routerError->SetAttribute("ErrorRate", DoubleValue(0.001));
    senderRouterLink.Get(1)->SetAttribute("ReceiveErrorModel", PointerValue(routerError));

    // Set link characteristics for routerReceiver
    link.SetDeviceAttribute("DataRate", StringValue("7Mbps"));
    link.SetChannelAttribute("Delay", StringValue("8ms"));

    // Create the link and set receiver error model
    NetDeviceContainer routerReceiverLink = link.Install(routerReceiver);

    Ptr<RateErrorModel> receiverError = CreateObject<RateErrorModel>();
    receiverError->SetAttribute("ErrorRate", DoubleValue(0.000001));
    routerReceiverLink.Get(1)->SetAttribute("ReceiveErrorModel", PointerValue(receiverError));

    // Assign IP addresses to interfaces for sender router links.
    Ipv4AddressHelper inetAddr;
    inetAddr.SetBase ("10.1.1.0", "255.255.255.252");
    Ipv4InterfaceContainer senderRouterInterface = inetAddr.Assign(senderRouterLink);

    // Assign IP addresses to interfaces for router receiver links.
    inetAddr.SetBase("10.2.2.0", "255.255.255.0");
    Ipv4InterfaceContainer routerReceiverInterface = inetAddr.Assign(routerReceiverLink);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // start the receiver packer sink
    Address remoteAddr (InetSocketAddress(routerReceiverInterface.GetAddress(1), port));
    PacketSinkHelper sinkHelper (socketFactory, InetSocketAddress(Ipv4Address::GetAny(), port));

    ApplicationContainer sink = sinkHelper.Install(container.Get(2));
    sink.Start(Seconds(0.));
    sink.Stop(Seconds(60.));

    // create TCP socket for sender
    Ptr<Socket> senderSk = Socket::CreateSocket(container.Get(0), TcpSocketFactory::GetTypeId());

    // Run the network
    Ptr<TCPEyenetNetwork> network = CreateObject<TCPEyenetNetwork>();
    network->Setup(senderSk, remoteAddr, pktSize, nPkts, DataRate(dataRate));
    container.Get(0)->AddApplication(network);
    network->SetStartTime(Seconds(1.));
    network->SetStopTime(Seconds(60.));

    AsciiTraceHelper traceHelper;
    Ptr<OutputStreamWrapper> cwndStream = traceHelper.CreateFileStream("TCPEyenet-cwnd.csv");
    senderSk->TraceConnectWithoutContext("CongestionWindow", MakeBoundCallback(&CwndChange, cwndStream));

    link.EnablePcapAll("tcp-eyenet");

    Simulator::Stop(Seconds(stopTime));
    Simulator::Run();
    Simulator::Destroy();

    return EXIT_SUCCESS;
}
