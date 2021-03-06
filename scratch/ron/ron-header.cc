/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#include "ns3/packet.h"
#include "ns3/simulator.h"
#include "ns3/log.h"
#include "ns3/uinteger.h"
#include "ron-header.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("RonHeader");
NS_OBJECT_ENSURE_REGISTERED (RonHeader);

RonHeader::RonHeader ()
{
  NS_LOG_FUNCTION_NOARGS ();

  m_forward = false;
  m_nHops = 0;
  m_origin = 0;
  m_seq = 0;

  m_dest = 0;
  m_nIps = 0;
  m_ips = NULL;
}

RonHeader::RonHeader (Ipv4Address destination, Ipv4Address intermediate /*= Ipv4Address((uint32_t)0)*/)
{
  NS_LOG_FUNCTION (destination << " and " << intermediate);

  m_nHops = 0;
  m_origin = 0;
  m_seq = 0;

  m_dest = destination.Get ();
  
  if (intermediate.Get () != 0)
    {
      m_nIps = 1;
      m_ips = new uint32_t[1];
      m_ips[0] = intermediate.Get ();
      m_forward = true;
    }
  else
    {
      m_nIps = 0;
      m_ips = NULL;
      m_forward = false;
    }
}

RonHeader::RonHeader (const RonHeader& original)
{
  NS_LOG_FUNCTION (original.GetFinalDest ());

  m_forward = original.m_forward;
  m_nHops = original.m_nHops;
  m_nIps = original.m_nIps;
  m_seq = original.m_seq;
  m_dest = original.m_dest;
  m_origin = original.m_origin;
  
  // Deep copy buffer
  if (original.m_ips)
    {
      m_ips = new uint32_t[m_nIps];
      memcpy (m_ips, original.m_ips, m_nIps*4);
    }
  else
    {
      m_ips = NULL;
    }
}

RonHeader&
RonHeader::operator=(const RonHeader& original)
{
  NS_LOG_FUNCTION_NOARGS ();

  RonHeader temp ((const RonHeader)original);

  std::swap(m_forward, temp.m_forward);
  std::swap(m_nHops, temp.m_nHops);
  std::swap(m_nIps, temp.m_nIps);
  std::swap(m_seq, temp.m_seq);
  std::swap(m_dest, temp.m_dest);
  std::swap(m_origin, temp.m_origin);
  std::swap(m_ips, temp.m_ips);

  return *this;
}

/*
RonHeader::RonHeader (Ipv4Address destination, 
{
  RonHeader (destination);
  m_forward = true;
  m_nIps = 1;
  m_ips = new uint32_t[1];
  m_ips[0] = intermediate.Get ();
  }*/

RonHeader::~RonHeader ()
{
  NS_LOG_FUNCTION_NOARGS ();

  if (m_ips)
    delete m_ips;
}

TypeId
RonHeader::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::RonHeader")
    .SetParent<Header> ()
    .AddConstructor<RonHeader> ()
  ;
  return tid;
}

TypeId
RonHeader::GetInstanceTypeId (void) const
{
  return GetTypeId ();
}

uint8_t
RonHeader::GetHop (void) const
{
  return m_nHops;
}

Ipv4Address
RonHeader::GetFinalDest (void) const
{
  return Ipv4Address (m_dest);
}
 
Ipv4Address
RonHeader::GetNextDest (void) const
{
  NS_LOG_FUNCTION_NOARGS ();

  if (m_nHops < m_nIps)
    {
      uint32_t next = m_ips[m_nHops];
      return Ipv4Address (next);
    }
  else
    return GetFinalDest ();
}

Ipv4Address
RonHeader::GetOrigin (void) const
{
  return Ipv4Address (m_origin);
}

uint8_t
RonHeader::IncrHops (void)
{
  return ++m_nHops;
}

uint32_t
RonHeader::GetSeq (void) const
{
  return m_seq;
}

void
RonHeader::SetSeq (uint32_t seq)
{
  m_seq = seq;
}

/*Ipv4Address
RonHeader::PopNextDest (void)
{
  Ipv4Address next = GetNextDest ();

  uint32_t buff = new uint32_t[--m_nIps];
  memcpy(buff, m_ips, m_nIps);

  delete[] m_ips;
  m_ips = buff;

  return next;
  }*/

bool
RonHeader::IsForward (void) const
{
  return m_forward;
}

void
RonHeader::AddDest (Ipv4Address addr)
{
  NS_LOG_FUNCTION (addr);

  uint32_t next = addr.Get ();
  uint32_t *buff = new uint32_t[++m_nIps];

  if (m_ips)
    {      
      memcpy(buff, m_ips, (m_nIps-1)*4);
      delete m_ips;
    }

  buff[m_nIps-1] = next;
  m_ips = buff;
}

void
RonHeader::SetDestination (Ipv4Address dest)
{
  m_dest = dest.Get ();
}

void
RonHeader::SetOrigin (Ipv4Address origin)
{
  m_origin = origin.Get ();
}

void
RonHeader::ReversePath (void)
{
  NS_LOG_FUNCTION_NOARGS ();

  // Iterate over buffer and swap elements
  for (uint8_t i = m_nIps; i < m_nIps/2; i++)
    {
      uint32_t tmp = m_ips[i];
      m_ips[i] = m_ips[m_nIps - 1 - i];
      m_ips[m_nIps - 1 - i] = tmp;
    }

  uint32_t oldDest = m_dest;
  m_dest = m_origin;
  m_origin = oldDest;
  m_nHops = 0;
}

void
RonHeader::Print (std::ostream &os) const
{
  // This method is invoked by the packet printing
  // routines to print the content of my header.
  //os << "data=" << m_data << std::endl;
  os << "Packet " << m_seq << " from " << Ipv4Address (m_origin) << " to " << Ipv4Address (m_dest);
  if (m_forward)
    {
      os << " on hop # " << m_nHops << " with next destination " << GetNextDest ();
    }
}

uint32_t
RonHeader::GetSerializedSize (void) const
{
  // we reserve 2 bytes for our header.
  return RON_HEADER_SIZE(m_nIps);
}

void
RonHeader::Serialize (Buffer::Iterator start) const
{
  NS_LOG_FUNCTION_NOARGS ();

  start.WriteU8 (m_forward);
  start.WriteU8 (m_nHops);
  start.WriteU8 (m_nIps);
  start.WriteU32 (m_seq);
  start.WriteU32 (m_dest);
  start.WriteU32 (m_origin);

  if (m_nIps > 0 and m_ips != NULL)
    {
      start.Write ((uint8_t*)m_ips, m_nIps*4);
    }
}

uint32_t
RonHeader::Deserialize (Buffer::Iterator start)
{
  NS_LOG_FUNCTION_NOARGS ();

  m_forward = (bool)start.ReadU8 ();
  m_nHops = start.ReadU8 ();
  m_nIps = start.ReadU8 ();
  m_seq = start.ReadU32 ();
  m_dest = start.ReadU32 ();
  m_origin = start.ReadU32 ();

  if (m_ips)
    delete m_ips;
  m_ips = new uint32_t[m_nIps];
  
  start.Read ((uint8_t*)m_ips, m_nIps*4);

  // we return the number of bytes effectively read.
  return RON_HEADER_SIZE(m_nIps);
}

} //namespace ns3
