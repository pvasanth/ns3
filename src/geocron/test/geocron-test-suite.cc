/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

// Include a header file from your module to test.
#include "ns3/geocron-experiment.h"

// An essential include is test.h
#include "ns3/test.h"
#include "ns3/core-module.h"

#include "ns3/point-to-point-layout-module.h"

#include <vector>

// Do not put your test classes in namespace ns3.  You may find it useful
// to use the using directive to access the ns3 namespace directly
using namespace ns3;

typedef std::vector<Ptr<RonPeerEntry> > PeerContainer;

class GridGenerator
{
public:
  Ipv4AddressHelper rowHelper;
  Ipv4AddressHelper colHelper;

  PeerContainer cachedPeers;
  NodeContainer cachedNodes;

  static GridGenerator GetInstance ()
  {
    static GridGenerator instance = GridGenerator ();
    return instance;
  }
  
  static NodeContainer GetNodes ()
  {
    return GetInstance ().cachedNodes;
  }

  static PeerContainer GetPeers ()
  {
    return GetInstance ().cachedPeers;
  }

  GridGenerator ()
  {
    rowHelper = Ipv4AddressHelper ("10.1.0.0", "255.255.0.0");
    colHelper = Ipv4AddressHelper ("10.128.0.0", "255.255.0.0");

   //first, build a topology for us to use
    uint32_t nrows = 5, ncols = 5;
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
    pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));
    PointToPointGridHelper grid = PointToPointGridHelper (nrows, ncols, pointToPoint);
    grid.BoundingBox (0, 0, 10, 10);
    InternetStackHelper internet;
    grid.InstallStack (internet);
    grid.AssignIpv4Addresses (rowHelper, colHelper);
    rowHelper.NewNetwork ();
    colHelper.NewNetwork ();
  
    cachedNodes.Add (grid.GetNode (0,0));
    cachedNodes.Add (grid.GetNode (1,1));
    cachedNodes.Add (grid.GetNode (4,1));
    cachedNodes.Add (grid.GetNode (0,4)); //should be best ortho path for 0,0 --> 4,4
    cachedNodes.Add (grid.GetNode (3,3));
    cachedNodes.Add (grid.GetNode (4,4));

    for (NodeContainer::Iterator itr = cachedNodes.Begin (); itr != cachedNodes.End (); itr++)
      {
        Ptr<RonPeerEntry> newPeer = Create<RonPeerEntry> (*itr);
        //newPeer->id = id++;
        cachedPeers.push_back (newPeer);
      }
  }
};

////////////////////////////////////////////////////////////////////////////////
class TestPeerEntry : public TestCase
{
public:
  TestPeerEntry ();
  virtual ~TestPeerEntry ();
  NodeContainer nodes;
  PeerContainer peers;

private:
  virtual void DoRun (void);
};

TestPeerEntry::TestPeerEntry ()
  : TestCase ("Tests basic operations of RonPeerEntry objects")
{
  nodes = GridGenerator::GetNodes ();
  peers = GridGenerator::GetPeers ();
}

TestPeerEntry::~TestPeerEntry ()
{
}

void
TestPeerEntry::DoRun (void)
{
  //NS_TEST_ASSERT_MSG_EQ (false, true, "got here");

  //Test equality operator
  bool equality = (*peers[0] == *peers[0]);
  NS_TEST_ASSERT_MSG_EQ (equality, true, "equality test same ptr failed");
  
  equality = (*peers[3] == *peers[3]);
  NS_TEST_ASSERT_MSG_EQ (equality, true, "equality test same ptr failed");

  equality = (*peers[2] == *(Create<RonPeerEntry> (nodes.Get (2))));
  NS_TEST_ASSERT_MSG_EQ (equality, true, "equality test new ptr failed");
  
  equality = (*peers[3] == *peers[1]);
  NS_TEST_ASSERT_MSG_NE (equality, true, "equality test false positive");

  //Test < operator
  equality = *peers[0] < *peers[4];
  NS_TEST_ASSERT_MSG_EQ (equality, true, "< operator test");

  equality = *peers[0] < *peers[0];
  NS_TEST_ASSERT_MSG_EQ (equality, false, "< operator test with self");

  equality = *peers[4] < *peers[1];
  NS_TEST_ASSERT_MSG_EQ (equality, false, "< operator test");
}


////////////////////////////////////////////////////////////////////////////////
class TestPeerTable : public TestCase
{
public:
  TestPeerTable ();
  virtual ~TestPeerTable ();
  NodeContainer nodes;
  PeerContainer peers;

private:
  virtual void DoRun (void);
};


TestPeerTable::TestPeerTable ()
  : TestCase ("Test basic operations of PeerTable objects")
{
  nodes = GridGenerator::GetNodes ();
  peers = GridGenerator::GetPeers ();
}

TestPeerTable::~TestPeerTable ()
{
}

void
TestPeerTable::DoRun (void)
{
  Ptr<RonPeerTable> table = Create<RonPeerTable> ();

  Ptr<RonPeerEntry> peer0 = peers[0];
  table->AddPeer (peer0);
  table->AddPeer (peers[1]);
  table->AddPeer (nodes.Get (3));

  bool equality = *peers[3] == *table->GetPeer (nodes.Get (3)->GetId ());
  NS_TEST_ASSERT_MSG_EQ (equality, true, "getting peer (added as node) by id");

  equality = *table->GetPeerByAddress (peer0->address) == *peer0;
  NS_TEST_ASSERT_MSG_EQ (equality, true, "getting peer by address");

  equality = *table->GetPeerByAddress (peer0->address) == *peers[1];
  NS_TEST_ASSERT_MSG_EQ (equality, false, "getting peer by address, testing false positive");

  equality = NULL == table->GetPeer (nodes.Get (4)->GetId ());
  NS_TEST_ASSERT_MSG_EQ (equality, true, "table should return NULL when peer not present");

  /*TODO: test removal
  equality = table->RemovePeer (nodes.Get (4)->GetId ());
  NS_TEST_ASSERT_MSG_NE (equality, true, "remove should only return true if peer existed");

  equality = table->RemovePeer (nodes.Get (3)->GetId ());
  NS_TEST_ASSERT_MSG_EQ (equality, true, "remove should only return true if peer existed");*/
}


////////////////////////////////////////////////////////////////////////////////
class TestRonPath : public TestCase
{
public:
  TestRonPath ();
  virtual ~TestRonPath ();
  NodeContainer nodes;
  PeerContainer peers;

private:
  virtual void DoRun (void);
};


TestRonPath::TestRonPath ()
  : TestCase ("Test basic operations of RonPath objects")
{
  nodes = GridGenerator::GetNodes ();
  peers = GridGenerator::GetPeers ();
}

TestRonPath::~TestRonPath ()
{
}

void
TestRonPath::DoRun (void)
{
  Ptr<PeerDestination> start = Create<PeerDestination> (peers[0]);
  Ptr<PeerDestination> end = Create<PeerDestination> (peers.back ());

  ////////////////////////////// TEST PeerDestination  ////////////////////

  bool equality = *end == *end;
  NS_TEST_ASSERT_MSG_EQ (equality, true, "testing PeerDestination equality same obj");

  equality = *start == *end;
  NS_TEST_ASSERT_MSG_EQ (equality, false, "testing PeerDestination equality false positive");
  
  equality = *start == *Create<PeerDestination> (peers[3]);
  NS_TEST_ASSERT_MSG_EQ (equality, false, "testing PeerDestination equality false positive new copy");
  
  equality = *start == *Create<PeerDestination> (peers.front());
  NS_TEST_ASSERT_MSG_EQ (equality, true, "testing PeerDestination equality new copy");
  
  equality = *start == *Create<PeerDestination> (Create<RonPeerEntry> (nodes.Get (0)));
  NS_TEST_ASSERT_MSG_EQ (equality, true, "testing PeerDestination equality new peer");

  equality = *(*start->Begin ()) == (*(*Create<PeerDestination> (Create<RonPeerEntry> (nodes.Get (0)))->Begin ()));
  NS_TEST_ASSERT_MSG_EQ (equality, true, "testing PeerDestination Iterator equality new copy");

  equality = *(*end->Begin ()) == (*(*Create<PeerDestination> (peers[0])->Begin ()));
  NS_TEST_ASSERT_MSG_EQ (equality, false, "testing PeerDestination Iterator false positive equality new copy");

  equality = *(*start->Begin ()) == (*(*Create<PeerDestination> (peers[0])->Begin ()));
  NS_TEST_ASSERT_MSG_EQ (equality, true, "testing PeerDestination Iterator equality new copy");

  NS_TEST_ASSERT_MSG_EQ (start->GetN (), 1, "testing PeerDestination.GetN");

  //TODO: multi-peer destinations, which requires some ordering or comparing


  ////////////////////////////// TEST PATH  ////////////////////

  Ptr<RonPath> path0 = Create<RonPath> (start);
  Ptr<RonPath> path1 = Create<RonPath> (end);
  Ptr<RonPath> path2 = Create<RonPath> (peers[3]);
  Ptr<RonPath> path3 = Create<RonPath> (Create<RonPeerEntry> (nodes.Get (0)));
  Ptr<RonPath> path4 = Create<RonPath> (peers.back());

  equality = *path0 == *path0;
  NS_TEST_ASSERT_MSG_EQ (equality, true, "testing RonPath equality with self");

  equality = *path0 == *Create<RonPath> (start);
  NS_TEST_ASSERT_MSG_EQ (equality, true, "testing RonPath equality with fresh path, same init");

  equality = *path0 == *Create<RonPath> (peers[0]);
  NS_TEST_ASSERT_MSG_EQ (equality, true, "testing RonPath equality with fresh path, same init re-retrieved");

  equality = *path4 == *path1;
  NS_TEST_ASSERT_MSG_EQ (equality, true, "testing RonPath equality with fresh path, one made by peer");

  equality = *path0 == *path3;
  NS_TEST_ASSERT_MSG_EQ (equality, true, "testing RonPath equality with fresh path, also fresh PeerDestination and RonPeerEntry");

  //try accessing elements and testing for equality

  equality = *path0->GetDestination () == *path0->GetDestination ();
  NS_TEST_ASSERT_MSG_EQ (equality, true, "testing for consistent self equality of destination");

  equality = *path0->GetDestination () == *path1->GetDestination ();
  NS_TEST_ASSERT_MSG_EQ (equality, false, "testing for destination false positive");

  equality = *start == *(*path0->Begin ());
  NS_TEST_ASSERT_MSG_EQ (equality, true, "testing equality with Begin and PeerDestination");

  equality = *(Create<PeerDestination> (peers.back ())) == *(*path1->Begin ());
  NS_TEST_ASSERT_MSG_EQ (equality, true, "testing equality with Begin and fresh PeerDestination");

  equality = *path0 == *path1;
  NS_TEST_ASSERT_MSG_EQ (equality, false, "testing false positive equality on separately built paths (start,end)");

  //now add a hop
  path0->AddHop (end);

  Ptr<PeerDestination> oldBegin = (*(path0->Begin ()));
  Ptr<PeerDestination> newBegin, newEnd, oldEnd;

  equality = *oldBegin == *(*path0->Begin ());
  NS_TEST_ASSERT_MSG_EQ (equality, true, "testing for consistent path start after adding a PeerDestination");

  equality = *path0->GetDestination () == *path0->GetDestination ();
  NS_TEST_ASSERT_MSG_EQ (equality, true, "testing for consistent self equality of destination after adding");

  equality = *path0->GetDestination () == *path1->GetDestination ();
  NS_TEST_ASSERT_MSG_EQ (equality, true, "testing for GetDestination equality of two paths after adding to one");

  equality = *(path1->GetDestination ()) == *(Create<RonPath> (end)->GetDestination ());
  NS_TEST_ASSERT_MSG_EQ (equality, true, "testing same dest fresh copy before adding");

  equality = *(*path0->Begin ()) == *(*Create<RonPath> (start)->Begin ());
  NS_TEST_ASSERT_MSG_EQ (equality, true, "testing same start fresh copy after adding");

  Ptr<RonPath> tempPath = Create<RonPath> (start);
  tempPath->AddHop (end);
  equality = *(path0->GetDestination ()) == *(tempPath->GetDestination ());
  NS_TEST_ASSERT_MSG_EQ (equality, true, "testing same dest fresh copy after adding to both");

  equality = 2 == path0->GetN ();
  NS_TEST_ASSERT_MSG_EQ (equality, true, "testing for consistent path size after adding a PeerDestination");

  equality = 1 == path1->GetN ();
  NS_TEST_ASSERT_MSG_EQ (equality, true, "testing for consistent path size of other path");

  oldBegin = *(path1->Begin ());
  newBegin = (path0->GetDestination ());

  equality = (*newBegin == *oldBegin);
  NS_TEST_ASSERT_MSG_EQ (equality, true, "testing for PeerDestination in one path after added from another using GetDestination");

  equality = (*newBegin == *path1->GetDestination ());
  NS_TEST_ASSERT_MSG_EQ (equality, true, "testing for PeerDestination in one path after added from another using GetDestination");

  equality = (*path0 == *Create<RonPath> (start));
  NS_TEST_ASSERT_MSG_EQ (equality, false, "testing for PeerDestination false positive equality after adding PeerDestination");

  equality = (*path0 == *Create<RonPath> (Create<PeerDestination> (Create<RonPeerEntry> (nodes.Get (0)))));
  NS_TEST_ASSERT_MSG_EQ (equality, false, "testing for PeerDestination false positive equality after adding PeerDestination fresh PeerDestination");

  equality = (*path0 == *Create<RonPath> (Create<PeerDestination> (peers.back ())));
  NS_TEST_ASSERT_MSG_EQ (equality, false, "testing for PeerDestination false positive equality after adding PeerDestination fresh PeerDestination, checking back");

  oldBegin = *(path1->Begin ());
  RonPath::Iterator pathItr1 = path0->Begin ();
  pathItr1++;
  newBegin = *(pathItr1);

  equality = (*newBegin == *oldBegin);
  NS_TEST_ASSERT_MSG_EQ (equality, true, "testing for PeerDestination in one path after added from another using Iterator");

  //paths 0 and 1 should be equal now
  oldEnd = path1->GetDestination ();
  path1->AddHop (start, path1->Begin ());

  equality = *start == *(*path1->Begin ());
  NS_TEST_ASSERT_MSG_EQ (equality, true, "testing expected start after inserting to front of path");

  equality = *oldEnd == *path1->GetDestination ();
  NS_TEST_ASSERT_MSG_EQ (equality, true, "testing destination after inserting to front of path");

  equality = *path0 == *path0;
  NS_TEST_ASSERT_MSG_EQ (equality, true, "testing same path now that it's same as another");

  equality = *(path1->GetDestination ()) == *(Create<RonPath> (end)->GetDestination ());
  NS_TEST_ASSERT_MSG_EQ (equality, true, "testing same dest fresh copy");

  equality = *path1->GetDestination () == *Create<RonPath> (peers.back ())->GetDestination ();
  NS_TEST_ASSERT_MSG_EQ (equality, true, "testing same dest deeper fresh copy");

  pathItr1 = path0->Begin ();
  RonPath::Iterator pathItr2 = path1->Begin ();
  equality = *(*(pathItr1)) == *(*(pathItr2));
  NS_TEST_ASSERT_MSG_EQ (equality, true, "testing begin iterator equality separately built paths (start,end)");

  equality = *path1->GetDestination () == *path0->GetDestination ();
  NS_TEST_ASSERT_MSG_EQ (equality, true, "testing same destination now that paths are same");

  equality = *(*(pathItr1)) == *(*(path2->Begin ()));
  NS_TEST_ASSERT_MSG_EQ (equality, false, "testing iterator equality false positive separately built paths (start,end)");

  pathItr1++;
  pathItr2++;
  equality = *(*(pathItr1)) == *(*(pathItr2));
  NS_TEST_ASSERT_MSG_EQ (equality, true, "testing iterator equality after incrementing separately built paths (start,end)");

  equality = *path0 == *path1;
  NS_TEST_ASSERT_MSG_EQ (equality, true, "separately built paths (start,end)");

  //check larger paths
  path1->AddHop (peers[3]);
  
  equality = *path0 == *path1;
  NS_TEST_ASSERT_MSG_EQ (equality, false, "testing proper equality for separately built up path subsets");

  //time to test reverse
  path2->AddHop (end);
  path2->AddHop (start);
  path2->Reverse ();

  equality = *path1 == *path2;
  NS_TEST_ASSERT_MSG_EQ (equality, true, "testing proper equality for reverse on separately built paths");
}


////////////////////////////////////////////////////////////////////////////////
class TestRonPathHeuristic : public TestCase
{
public:
  TestRonPathHeuristic ();
  virtual ~TestRonPathHeuristic ();
  NodeContainer nodes;
  PeerContainer peers;

private:
  virtual void DoRun (void);
};


TestRonPathHeuristic::TestRonPathHeuristic ()
  : TestCase ("Test basic operations of RonPathHeuristic objects as well as aggregation")
{
  nodes = GridGenerator::GetNodes ();
  peers = GridGenerator::GetPeers ();
}

TestRonPathHeuristic::~TestRonPathHeuristic ()
{
}

void
TestRonPathHeuristic::DoRun (void)
{
}


////////////////////////////////////////////////////////////////////////////////
class TestRonHeader : public TestCase
{
public:
  TestRonHeader ();
  virtual ~TestRonHeader ();

private:
  virtual void DoRun (void);
};


TestRonHeader::TestRonHeader ()
  : TestCase ("Test basic operations of Ronheader objects")
{
}

TestRonHeader::~TestRonHeader ()
{
}

void
TestRonHeader::DoRun (void)
{
  // A wide variety of test macros are available in src/core/test.h
  NS_TEST_ASSERT_MSG_EQ (true, true, "true doesn't equal true for some reason");
  // Use this one for floating point comparisons
  NS_TEST_ASSERT_MSG_EQ_TOL (0.01, 0.01, 0.001, "Numbers are not equal within tolerance");
}


////////////////////////////////////////////////////////////////////////////////
////////////////////$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$////////////////////
//////////$$$$$$$$$$   End of test cases - create test suite $$$$$$$$$$/////////
////////////////////$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$////////////////////
////////////////////////////////////////////////////////////////////////////////


// The TestSuite class names the TestSuite, identifies what type of TestSuite,
// and enables the TestCases to be run.  Typically, only the constructor for
// this class must be defined
//
class GeocronTestSuite : public TestSuite
{
public:
  GeocronTestSuite ();
};

GeocronTestSuite::GeocronTestSuite ()
  : TestSuite ("geocron", UNIT)
{
  //basic data structs
  AddTestCase (new TestPeerEntry);
  AddTestCase (new TestPeerTable);
  AddTestCase (new TestRonPath);
  AddTestCase (new TestRonPathHeuristic);

  //network application stuff
  AddTestCase (new TestRonHeader);
}

// Do not forget to allocate an instance of this TestSuite
static GeocronTestSuite geocronTestSuite;

