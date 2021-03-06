/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright 2007 University of Washington
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "ns3/log.h"
#include "ns3/ipv4-address.h"
#include "ns3/address-utils.h"
#include "ns3/nstime.h"
#include "ns3/inet-socket-address.h"
#include "ns3/socket.h"
#include "ns3/udp-socket.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "ns3/packet.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/uinteger.h"
#include "ns3/ipv4.h"
#include "ns3/random-variable.h"

#include "ron-trace-functions.h"
#include "ron-client.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("RonClientApplication");
NS_OBJECT_ENSURE_REGISTERED (RonClient);

 
static UniformVariable random;  //for picking overlay nodes

TypeId
RonClient::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::RonClient")
    .SetParent<Application> ()
    .AddConstructor<RonClient> ()
    .AddAttribute ("Port", "Port on which we listen for incoming packets from overlay nodes.",
                   UintegerValue (9),
                   MakeUintegerAccessor (&RonClient::m_port),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("RemoteAddress", 
                   "The destination Ipv4Address of the outbound packets",
                   Ipv4AddressValue (),
                   MakeIpv4AddressAccessor (&RonClient::m_servAddress),
                   MakeIpv4AddressChecker ())
    .AddAttribute ("Timeout", "Time to wait for a reply before trying to resend or send along an overlay node.",
                   TimeValue (Seconds (3)),
                   MakeTimeAccessor (&RonClient::m_timeout),
                   MakeTimeChecker ())
    .AddAttribute ("MaxPackets", 
                   "The maximum number of packets the application will send",
                   UintegerValue (100),
                   MakeUintegerAccessor (&RonClient::m_count),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("Interval", 
                   "The time to wait between packets",
                   TimeValue (Seconds (1.0)),
                   MakeTimeAccessor (&RonClient::m_interval),
                   MakeTimeChecker ())
    .AddAttribute ("PacketSize", "Size of data in outbound packets",
                   UintegerValue (100),
                   MakeUintegerAccessor (&RonClient::SetDataSize,
                                         &RonClient::GetDataSize),
                   MakeUintegerChecker<uint32_t> ())
    .AddTraceSource ("Send", "A new packet is created and is sent to the server or an overlay node",
                     MakeTraceSourceAccessor (&RonClient::m_sendTrace))
    .AddTraceSource ("Ack", "An ACK packet originating from the server is received",
                     MakeTraceSourceAccessor (&RonClient::m_ackTrace))
    .AddTraceSource ("Forward", "A packet originating from an overlay node is received and forwarded to the server",
                     MakeTraceSourceAccessor (&RonClient::m_forwardTrace))
  ;
  return tid;
}

RonClient::RonClient ()
{
  NS_LOG_FUNCTION_NOARGS ();
  SetDefaults ();

  m_data = NULL;
  m_peers = Create<RonPeerTable> ();
  m_address = Ipv4Address ((uint32_t)0);
}

void
RonClient::SetDefaults ()
{
  m_sent = 0;
  m_socket = 0;
  m_nextPeer = 0;
  m_count = 0;
}


void
RonClient::DoReset ()
{
  NS_LOG_FUNCTION_NOARGS ();

  SetDefaults ();

  // Reschedule start so that StartApplication gets rescheduled
  // WARNING: was ScheduleWithContext (GetId (), args...) for in a node
  Simulator::Schedule (Seconds (0.0), 
                       &Application::Start, this);

  //Ipv4Address m_servAddress; //HANDLE SETTING!!
  CancelEvents ();
  m_outstandingSeqs.clear ();
}

RonClient::~RonClient()
{
  NS_LOG_FUNCTION_NOARGS ();
  m_socket = 0;

  delete [] m_data;

  m_data = 0;
  m_dataSize = 0;
}

void 
RonClient::SetRemote (Ipv4Address ip, uint16_t port)
{
  m_servAddress = ip;
  //m_port = port; //should already be set
}

void
RonClient::DoDispose (void)
{
  NS_LOG_FUNCTION_NOARGS ();
  Application::DoDispose ();
}

void 
RonClient::StartApplication (void)
{
  NS_LOG_FUNCTION_NOARGS ();

  if (m_startTime >= m_stopTime)
    {
      NS_LOG_LOGIC ("Cancelling application (start > stop)");
      //DoDispose ();
      return;
    }

  TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");

  if (m_socket == 0)
    {
      TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
      m_socket = Socket::CreateSocket (GetNode (), tid);
      InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), m_port);
      m_socket->Bind (local);

      NS_LOG_LOGIC ("Socket bound");

      /*if (addressUtils::IsMulticast (m_local))
        {
          Ptr<UdpSocket> udpSocket = DynamicCast<UdpSocket> (m_socket);
          if (udpSocket)
            {
              // equivalent to setsockopt (MCAST_JOIN_GROUP)
              udpSocket->MulticastJoinGroup (0, m_local);
            }
          else
            {
              NS_FATAL_ERROR ("Error: joining multicast on a non-UDP socket");
            }
            }*/
    }

  // Use the address of the first non-loopback device on the node for our address
  if (m_address.Get () == 0)
    {
      m_address = GetNode ()->GetObject<Ipv4> ()->GetAddress (1,0).GetLocal ();
    }

  m_socket->SetRecvCallback (MakeCallback (&RonClient::HandleRead, this));

  if (m_sent < m_count)
    ScheduleTransmit (Seconds (0.));
}

void 
RonClient::StopApplication ()
{
  NS_LOG_FUNCTION_NOARGS ();

  if (m_socket != 0) 
    {
      m_socket->Close ();
      m_socket->SetRecvCallback (MakeNullCallback<void, Ptr<Socket> > ());
      m_socket = 0;
    }

  CancelEvents ();
  Reset ();
}

void
RonClient::CancelEvents ()
{
  NS_LOG_FUNCTION_NOARGS ();

  for (std::list<EventId>::iterator itr = m_events.begin ();
       itr != m_events.end (); itr++)
    Simulator::Cancel (*itr);

  m_events.clear ();
}

void 
RonClient::SetFill (std::string fill)
{
  NS_LOG_FUNCTION (fill);

  uint32_t dataSize = fill.size () + 1;

  if (dataSize != m_dataSize)
    {
      if (m_data)
        delete [] m_data;
      m_data = new uint8_t [dataSize];
      m_dataSize = dataSize;
    }

  memcpy (m_data, fill.c_str (), dataSize);

  //
  // Overwrite packet size attribute.
  //
  m_size = dataSize;
}

void 
RonClient::SetFill (uint8_t fill, uint32_t dataSize)
{
  if (dataSize != m_dataSize)
    {
      if (m_data)
        delete [] m_data;
      m_data = new uint8_t [dataSize];
      m_dataSize = dataSize;
    }

  memset (m_data, fill, dataSize);

  //
  // Overwrite packet size attribute.
  //
  m_size = dataSize;
}

void 
RonClient::SetFill (uint8_t *fill, uint32_t fillSize, uint32_t dataSize)
{
  if (dataSize != m_dataSize)
    {
      if (m_data)
        delete [] m_data;
      m_data = new uint8_t [dataSize];
      m_dataSize = dataSize;
    }

  if (fillSize >= dataSize)
    {
      memcpy (m_data, fill, dataSize);
      return;
    }

  //
  // Do all but the final fill.
  //
  uint32_t filled = 0;
  while (filled + fillSize < dataSize)
    {
      memcpy (&m_data[filled], fill, fillSize);
      filled += fillSize;
    }

  //
  // Last fill may be partial
  //
  memcpy (&m_data[filled], fill, dataSize - filled);

  //
  // Overwrite packet size attribute.
  //
  m_size = dataSize;
}

void 
RonClient::SetDataSize (uint32_t dataSize)
{
  NS_LOG_FUNCTION (dataSize);

  //
  // If the client is setting the echo packet data size this way, we infer
  // that she doesn't care about the contents of the packet at all, so 
  // neither will we.
  //
  if (m_data)
    delete [] m_data;
  m_data = 0;
  m_dataSize = 0;
  m_size = dataSize;
}

uint32_t 
RonClient::GetDataSize (void) const
{
  NS_LOG_FUNCTION_NOARGS ();
  return m_size;
}

void 
RonClient::ScheduleTransmit (Time dt, bool viaOverlay /*= false*/)
{
  NS_LOG_FUNCTION_NOARGS ();
  m_events.push_front (Simulator::Schedule (dt, &RonClient::Send, this, viaOverlay));
}

void 
RonClient::Send (bool viaOverlay)
{
  NS_LOG_FUNCTION_NOARGS ();

  NS_LOG_LOGIC ("Sending " << (viaOverlay ? "indirect" : "direct") << " packet.");

  Ptr<Packet> p;
  if (m_dataSize)
    {
      //
      // If m_dataSize is non-zero, we have a data buffer of the same size that we
      // are expected to copy and send.  This state of affairs is created if one of
      // the Fill functions is called.  In this case, m_size must have been set
      // to agree with m_dataSize
      //
      NS_ASSERT_MSG (m_dataSize == m_size, "RonClient::Send(): m_size and m_dataSize inconsistent");
      NS_ASSERT_MSG (m_data, "RonClient::Send(): m_dataSize but no m_data");
      p = Create<Packet> (m_data, m_dataSize);
    }
  else
    {
      //
      // If m_dataSize is zero, the client has indicated that she doesn't care 
      // about the data itself either by specifying the data size by setting
      // the corresponding atribute or by not calling a SetFill function.  In 
      // this case, we don't worry about it either.  But we do allow m_size
      // to have a value different from the (zero) m_dataSize.
      //
      p = Create<Packet> (m_size);
    }

  // add RON header to packet
  RonHeader head;

  // If forwarding thru overlay, just pick a peer at random from those available
  if (viaOverlay)
    {
      Ipv4Address intermediary;
      try
        {
          intermediary = m_heuristic->GetNextPeerAddress (m_serverPeer);//m_peers[random.GetInteger (0, m_peers.size () - 1)];
        }
      catch (RonPathHeuristic::NoValidPeerException& e)
        {
          NS_LOG_DEBUG (e.what ());
          CancelEvents ();
          return;
        }

      //NS_LOG_INFO ("Trying to send along overlay node " << intermediary);
      head = RonHeader (m_servAddress, intermediary);
    }
  else
    head = RonHeader (m_servAddress);

  head.SetSeq (m_sent);
  head.SetOrigin (m_address);
  p->AddHeader (head);

  // call to the trace sinks before the packet is actually sent,
  // so that tags added to the packet can be sent as well
  m_sendTrace (p, GetNode ()->GetId ());
  m_socket->SendTo (p, 0, InetSocketAddress(head.GetNextDest (), m_port));
  ScheduleTimeout (m_sent++);

  //NS_LOG_INFO ("Sent " << m_size << " bytes to " << m_servAddress);

  // Currently we only attempt direct contact once
  /*if (m_sent < m_count) 
    {
      ScheduleTransmit (m_interval);
      }*/
}

void 
RonClient::HandleRead (Ptr<Socket> socket)
{
  Ptr<Packet> packet;
  Address from;

  while (packet = socket->RecvFrom (from))
    {
      NS_LOG_LOGIC ("Reading packet from socket.");

      if (InetSocketAddress::IsMatchingType (from))
        {
          Ipv4Address source = InetSocketAddress::ConvertFrom (from).GetIpv4 ();

          RonHeader head;
          packet->PeekHeader (head);

          // If the packet is for us, process the ACK
          if (head.GetFinalDest () == head.GetNextDest ())
            {
              ProcessAck (packet, source);
            }

          // Else forward it
          else
            {
              ForwardPacket (packet, source);
            }
        }
    }
}

void
RonClient::ForwardPacket (Ptr<Packet> packet, Ipv4Address source)
{
  NS_LOG_LOGIC ("Forwarding packet");

  packet->RemoveAllPacketTags ();
  packet->RemoveAllByteTags ();

  // Edit the packet's current RON header and get next hop
  RonHeader head;
  packet->RemoveHeader (head);

  // If this is the first hop on the route, we need to set the source
  // since NS3 doesn't give us a convenient way to access which interface
  // the original packet was sent out on.
  if (head.GetHop () == 0)
    head.SetOrigin (source);

  head.IncrHops ();
  Ipv4Address destination = head.GetNextDest ();
  packet->AddHeader (head);

  m_forwardTrace (packet, GetNode ()-> GetId ());
  m_socket->SendTo (packet, 0, InetSocketAddress(destination, m_port));
}

void
RonClient::ProcessAck (Ptr<Packet> packet, Ipv4Address source)
{
  NS_LOG_LOGIC ("ACK received");

  RonHeader head;
  packet->PeekHeader (head);
  uint32_t seq = head.GetSeq ();
  
  m_ackTrace (packet, GetNode ()-> GetId ());
  m_outstandingSeqs.erase (seq);
  //TODO: handle an ack from an old seq number

  CancelEvents ();
  //TODO: store path? send more data?
}

void
RonClient::ScheduleTimeout (uint32_t seq)
{
  m_events.push_front (Simulator::Schedule (m_timeout, &RonClient::CheckTimeout, this, seq));
  m_outstandingSeqs.insert (seq);
}

void
RonClient::AddPeer (Ptr<Node> node)
{
  //if (addr != m_address)
  m_peers->AddPeer (node);
}



void
RonClient::SetPeerTable (Ptr<RonPeerTable> peers)
{
  m_peers = peers;
}


void
RonClient::SetRemotePeer (Ptr<RonPeerEntry> peer)
{
  m_serverPeer = peer;
  m_servAddress = peer->address;
}


void
RonClient::SetHeuristic (Ptr<RonPathHeuristic> heuristic)
{
  m_heuristic = heuristic;
  heuristic->SetSourcePeer (Create<RonPeerEntry> (GetNode ()));
}

Ipv4Address
RonClient::GetAddress () const
{
  return m_address;
}

void
RonClient::CheckTimeout (uint32_t seq)
{
  std::set<uint32_t>::iterator itr = m_outstandingSeqs.find (seq);

  // If it's timed out, we should try a different path to the server
  if (itr != m_outstandingSeqs.end ())
    {
      NS_LOG_LOGIC ("Packet with seq# " << seq << " timed out.");
      m_outstandingSeqs.erase (itr);
      
      if (m_sent < m_count)
        ScheduleTransmit (Seconds (0.0), true);
    }
}


void
RonClient::ConnectTraces (Ptr<OutputStreamWrapper> traceOutputStream)
{
  this->TraceDisconnectWithoutContext ("Ack", m_ackcb);
  this->TraceDisconnectWithoutContext ("Send", m_sendcb);
  this->TraceDisconnectWithoutContext ("Forward", m_forwardcb);
  
  m_ackcb = MakeBoundCallback (&AckReceived, traceOutputStream);
  m_sendcb = MakeBoundCallback (&PacketSent, traceOutputStream);
  m_forwardcb = MakeBoundCallback (&PacketForwarded, traceOutputStream);

  this->TraceConnectWithoutContext ("Ack", m_ackcb);
  this->TraceConnectWithoutContext ("Forward", m_forwardcb);
  this->TraceConnectWithoutContext ("Send", m_sendcb);
}
  

} // Namespace ns3
