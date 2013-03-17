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

#include "ron-peer-table.h"
#include "failure-helper-functions.h"
#include "geocron-experiment.h"

#include <boost/range/functions.hpp>

using namespace ns3;

RonPeerEntry::RonPeerEntry () {}

RonPeerEntry::RonPeerEntry (Ptr<Node> node)
{
    this->node = node;
    id = node-> GetId ();
    address = GetNodeAddress(node);
    lastContact = Simulator::Now ();
    
    Ptr<MobilityModel> mobility = node->GetObject<MobilityModel> ();
    NS_ASSERT_MSG (mobility != NULL, "Geocron nodes need MobilityModels for determining locations!");    
    location = mobility->GetPosition ();
}

TypeId
RonPeerEntry::GetTypeId ()
{
  static TypeId tid = TypeId ("ns3::RonPeerEntry")
    .SetParent<Object> ()
    .AddConstructor<RonPeerEntry> ()
  ;
  return tid;
}


bool
RonPeerEntry::operator== (const RonPeerEntry rhs) const
{
  return id == rhs.id;
}


bool
RonPeerEntry::operator!= (const RonPeerEntry rhs) const
{
  return !(*this == rhs);
}

bool
RonPeerEntry::operator< (RonPeerEntry rhs) const
{
  return id < rhs.id;
}

////////////////////////////////////////////////////////////////////////////////

Ptr<RonPeerTable>
RonPeerTable::GetMaster ()
{
  static Ptr<RonPeerTable> m_master = Create<RonPeerTable> ();
  return m_master;
}


Ptr<RonPeerEntry>
RonPeerTable::GetPeerByAddress (Ipv4Address address)
{
  if (m_peersByAddress.count (address.Get ()))
    return m_peersByAddress[address.Get ()];
  else
    return NULL;
}


uint32_t
RonPeerTable::GetN ()
{
  return m_peers.size ();
}


Ptr<RonPeerEntry>
RonPeerTable::AddPeer (Ptr<RonPeerEntry> entry)
{
  Ptr<RonPeerEntry> returnValue;
  if (m_peers.count (entry->id) != 0)
    {
      Ptr<RonPeerEntry> temp = (*(m_peers.find (entry->id))).second;
      m_peers[entry->id] = entry;
      returnValue = temp;
    }
  else
    returnValue = m_peers[entry->id] = entry;
  
  m_peersByAddress[(entry->address.Get ())] = entry;

  return returnValue;
}


Ptr<RonPeerEntry>
RonPeerTable::AddPeer (Ptr<Node> node)
{
  Ptr<RonPeerEntry> newEntry = Create<RonPeerEntry> (node);
  return AddPeer (newEntry);
}


bool
RonPeerTable::RemovePeer (uint32_t id)
{
  bool retValue = false;
  if (m_peers.count (id))
    retValue = true;
  Ipv4Address addr = GetPeer (id)->address;
  m_peers.erase (m_peers.find (id));
  m_peersByAddress.erase (m_peersByAddress.find (addr.Get ()));
  return retValue;
}


Ptr<RonPeerEntry>
RonPeerTable::GetPeer (uint32_t id)
{
  if (m_peers.count (id))
    return  ((*(m_peers.find (id))).second);
  else
    return NULL;
}


bool
RonPeerTable::IsInTable (uint32_t id)
{
  return m_peers.count (id);
}


bool
RonPeerTable::IsInTable (Iterator itr)
{
  return IsInTable ((*itr)->id);
}


RonPeerTable::Iterator
RonPeerTable::Begin ()
{
  return boost::begin (boost::adaptors::values (m_peers));
/*template<typename T1, typename T2> T2& take_second(const std::pair<T1, T2> &a_pair)
{
  return a_pair.second;
}
boost::make_transform_iterator(a_map.begin(), take_second<int, string>),*/
}


RonPeerTable::Iterator
RonPeerTable::End ()
{
  return boost::end (boost::adaptors::values (m_peers));
}
