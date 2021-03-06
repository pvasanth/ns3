#! /usr/bin/python
RON_TRACE_ANALYZER_DESCRIPTION = '''A helper script for analyzing ns3 Resilient Overlay Network simulation traces and visualizing the data.'''
#
# (c) University of California Irvine 2012
# @author: Kyle Benson

import argparse, os.path, os, decimal, math, heapq, sys, scipy.stats, itertools #numpy

##################################################################################
#################      ARGUMENTS       ###########################################
# ArgumentParser.add_argument(name or flags...[, action][, nargs][, const][, default][, type][, choices][, required][, help][, metavar][, dest])
# action is one of: store[_const,_true,_false], append[_const], count
# nargs is one of: N, ?(defaults to const when no args), *, +, argparse.REMAINDER
# help supports %(var)s: help='default value is %(default)s'
##################################################################################

def ParseArgs():

    parser = argparse.ArgumentParser(description=RON_TRACE_ANALYZER_DESCRIPTION,
                                     #formatter_class=argparse.RawTextHelpFormatter
                                     epilog='(*1/N): These arguments will be applied to their respective graphs/groups in the order they are specified in.  If only one argument is given, it will be applied to all the groups.')

    # Input Traces
    parser.add_argument('--files', '-f', type=str, nargs='+',
                        help='''files from which to read trace data''')

    parser.add_argument('--dirs', '-d', type=str, nargs='+',
                        help='''directories from which to read trace data files\
    (directories should group together runs from different parameters being studied). \
    Hidden files (those starting with '.') and subdirectories are ignored (use them to store parameters/observations/graphs).''')

    # Labeling groups
    parser.add_argument('--label', '-l', nargs='+',
                        help='label each respective file or directory with these args instead of the file/directory names')

    parser.add_argument('--append_label', nargs='+',
                        help='(*1/N) Append the given arguments to the group labels given in the legend.')

    parser.add_argument('--prepend_label', nargs='+',
                        help='(*1/N) Prepend the given arguments to the group labels given in the legend.')

    # Titling graphics
    parser.add_argument('--title', nargs='+',
                        help='manually enter title(s) for the graph(s)')

    parser.add_argument('--append_title', nargs='+',
                        help='(*1/N) Append the given arguments to the title of the respective graphs.')

    parser.add_argument('--prepend_title', nargs='+',
                        help='(*1/N) Prepend the given arguments to the title of the respective graphs.')


    # Graph types to build
    parser.add_argument('--time', '-t', action='store_true',
                        help='show graph of total ACKs received over time')

    parser.add_argument('--congestion', '-c', action='store_true',
                        help='graph number of packets sent for each group')

    parser.add_argument('--improvement', '-i', action='store_true',
                        help='graph percent improvement over not using an overlay for each file or group')
    parser.add_argument('--utility', '-u',# nargs='?', const='fprob',
                        help='''Graph the computed utility metric value of each group vs. failure probabilitythe (optionally) specified x-axis argument (default=%(default)s).
Current options are:
  fprob''')


    # Graph output modifiers
    parser.add_argument('--average', '-a', action='store_true',
                        help='average together all files or groups instead of comparing them \
    (files within a group are always averaged unless --separate is specified) \
CURRENTLY NOT IMPLEMENTED*')

    parser.add_argument('--separate', action='store_true',
                        help='don\'t average together the runs within a directory: put them each in a separate group.')

    parser.add_argument('--resolution', type=float,
                        help='Time resolution (in seconds) for time-based graphs.')

    parser.add_argument('--no_save', '-ns', action='store_true',
                        help='Don\'t save the graphs automatically.')

    parser.add_argument('--no_windows', '-nw', action='store_true',
                        help='Don\'t display the graphs after creating them')

    parser.add_argument('--format', default='.svg',
                        help='Image format used to export graphs. Default: %(default)s')

    parser.add_argument('--output_directory', default='ron_output',
                        help='Base directory to output graphics files to. Default: %(default)s')

    # Naming files
    parser.add_argument('--names', nargs='+',
                        help='Explicitly name the respective graph files that will be saved.')

    parser.add_argument('--append_name', nargs='+',
                        help='Append arg to all output graph files saved on this run.')

    parser.add_argument('--prepend_name', nargs='+',
                        help='Prepend arg to all output graph files saved on this run.')

    # Text data
    parser.add_argument('--summary', '-s', action='store_true',
                        help='Print statistics summary about each file/group.  Includes total nodes, # ACKS, # direct ACKs')
    ## --utility will add those (expected/mean) values to summary
    parser.add_argument('--t_test', action='store_true',
                        help='Compute 2-sample t-test for every pair of groups, taken two at a time in the order specified.')

    return parser.parse_args()

#TODO: how to compare intersections/unions of different groups? set difference?

    #TODO: add something to do with the number of forwards during the event?
    #TODO: what about when we want to specify the x-axis for groups?
#TODO: how to find out the total number of overlay nodes and not just ones that participated???



############################## Helper Functions ##########################

def percentImprovement(nAcks, nDirectAcks):
    if nDirectAcks:
        return (nAcks - nDirectAcks) / float(nDirectAcks) * 100
    else:
        return float('inf')

def normalizedTimes(nNodes, timeCounts):
    '''Takes a node count and a TraceRun or TraceGroup getXTimes() function output (2-tuple) as input and 
    returns the normalized form of the outputs (divides each data point by the node count).'''
    
    return (timeCounts[0], [c/float(nNodes) for c in timeCounts[1]])


def cumulative(timeCounts):
    '''Takes a TraceRun or TraceGroup getXTimes() function output (2-tuple) as input and 
    returns the cumulative number instead of the exact number during that time slice at each index.'''

    times = timeCounts[0]
    counts = timeCounts[1]

    total = 0
    for i in range(min(len(times), len(counts))):
        if counts[i]:
            total += counts[i]
            counts[i] = total

    return (times, counts)


############################################################################################################
############################        Trace Objects        ###################################################
############################################################################################################


class TraceNode:
    '''
    Represents a single node during a simulation.
    Stores important values pertaining to it and can perform some calculations.
    '''
    def __init__(self,id):
        self.id = id
        self.acks = 0
        self.directAcks = 0
        self.firstAckTime = 0
        self.sends = 0
        self.forwards = 0

    # TODO: implement getting more than one ACK????

    def __repr__(self):
        return "Node " + self.id + ((" received first " + ("direct" if self.directAcks else "indirect") + " ACK at " + self.ackTime) if self.acks else "") + " sent %d and forwarded %d packets" % (self.sends, self.forwards)

    def getTotalSends(self):
        return self.sends + self.forwards

    def getUtility(self):
        # Don't count direct ACKs
        if self.directAcks:
            return None
        if self.firstAckTime:
            return 1.0/self.firstAckTime
        return 0


class Parameters:
    '''
    The parameters used for a simulation run.
    '''
    def __init__(self):
        self.fprob = 0.0
        self.heuristic = ''
        #TODO: everything else

    @staticmethod
    def parseFolderHierarchy(name):
        parts = name.split(os.path.sep)
        
        # cut off filename as its just the run #
        if not os.path.isdir(parts[-1]):
            parts = parts[:-1]

        params = Parameters()
        params.heuristic = parts[-1]
        params.fprob = float(parts[-2])

        return params


class TraceRun:
    '''
    A single simulation run from one file. Holds the nodes and performs calculations.
    '''
    # Indices of important data in case trace files change
    NODE_ID_INDEX = 1
    ACTION_INDEX = 2
    DIRECT_ACK_INDEX = 3
    TIME_INDEX = 6

    TIME_RESOLUTION = 0.1 #In seconds

    def __init__(self,filename):
        self.nodes = {}
        self.name = "Run" if not filename else os.path.split(filename)[1]
        self.nAcks = None
        self.nDirectAcks = None
        self.utility = None
        self.forwardTimes = {}
        self.ackTimes = {}
        self.sendTimes = {}
        self.nNodes = None

        self.params = Parameters.parseFolderHierarchy(filename)

        sigDigits = int(round(-math.log(TraceRun.TIME_RESOLUTION,10)))

        with open(filename) as f:

            for line in f.readlines():
                parsed = line.split()
                nodeId = parsed[TraceRun.NODE_ID_INDEX]
                node = self.nodes.get(nodeId)

                if len(parsed) > TraceRun.TIME_INDEX:
                    time = round(float(parsed[TraceRun.TIME_INDEX]), sigDigits)

                if not node:
                    node = self.nodes[nodeId] = TraceNode(nodeId)

                if 'ACK' in line:
                    node.acks += 1
                    self.ackTimes[time] = self.ackTimes.get(time,0) + 1

                    if not node.firstAckTime:
                        node.firstAckTime = time

                    if 'direct' == parsed[TraceRun.DIRECT_ACK_INDEX]:
                        node.directAcks += 1

                if 'forwarded' == parsed[TraceRun.ACTION_INDEX]:
                    node.forwards += 1
                    self.forwardTimes[time] = self.forwardTimes.get(time,0) + 1

                if 'sent' == parsed[TraceRun.ACTION_INDEX]:
                    node.sends += 1
                    self.sendTimes[time] = self.sendTimes.get(time,0) + 1

    def getNNodes(self):
        '''Number of nodes that attempted contact with the server.'''
        if not self.nNodes:
            self.nNodes = sum([1 for n in self.nodes.values() if n.sends])
        return self.nNodes

    def getNAcks(self):
        '''Number of nodes that received at least 1 ACK from the server.'''
        if not self.nAcks:
            self.nAcks = sum([1 for i in self.nodes.values() if i.acks])
        return self.nAcks

    def getNSends(self):
        if not self.nSends:
            self.nSends = sum([i.sends for i in self.nodes.values()])
        return self.nSends

    def getNDirectAcks(self):
        '''Number of nodes that received at least 1 direct ACK from the server (didn't utilize the overlay).'''
        if not self.nDirectAcks:
            self.nDirectAcks = sum([1 for i in self.nodes.values() if i.directAcks])
        return self.nDirectAcks

    def getSendTimes(self):
        '''
        Returns a two-tuple of lists where the first list is the time values and the second
        is the number of occurences in that time slice.
        '''
        if isinstance(self.sendTimes, dict):
            items = sorted(self.sendTimes.items())
            self.sendTimes = ([i for i,j in items], [j for i,j in items])
        return self.sendTimes

    #TODO: getUploadTimes ??

    def getAckTimes(self):
        '''
        Returns a two-tuple of lists where the first list is the time values and the second
        is the number of occurences in that time slice.
        '''
        if isinstance(self.ackTimes, dict):
            items = sorted(self.ackTimes.items())
            self.ackTimes = ([i for i,j in items], [j for i,j in items])
        return self.ackTimes
        #return (self.ackTimes.keys(), self.ackTimes.values())

    def getForwardTimes(self):
        '''
        Returns a two-tuple of lists where the first list is the time values and the second
        is the number of occurences in that time slice.
        '''
        if isinstance(self.forwardTimes, dict):
            items = sorted(self.forwardTimes.items())
            self.forwardTimes = ([i for i,j in items], [j for i,j in items])
        return self.forwardTimes

    def getUtility(self):
        if not self.utility:
            utilities = [n.getUtility() for n in self.nodes.values() if n.getUtility() is not None]
            self.utility = float(sum(utilities))/len(utilities)
        return self.utility

class TraceGroup:
    '''
    A group of several TraceRuns, which typically were found in a single folder
    and had the same simulation parameters.

    Note that the getNNodes and getNAcks accessor functions will return the average value
    over these 
    '''

    def __init__(self,folder=None):
        self.traces = []
        self.name = "Group" if not folder else [part.replace('_',' ') for part in reversed(folder.split(os.path.sep)) if part][0]
        self.nAcks = None
        self.nNodes = None
        self.nDirectAcks = None
        self.sendTimes = None
        self.ackTimes = None
        self.directAckTimes = None
        self.stdevNAcks = None
        self.utility = None

        if folder:
            for f in os.listdir(folder):
                if not f.startswith('.'):
                    self.traces.append(TraceRun(os.path.join(folder,f)))
        elif folder is not None:
            print folder, "is not a directory!"

    def __len__(self):
        return len(self.traces)

    def getNNodes(self):
        '''
        Get average number of nodes for all TraceRuns in this group.
        '''
        if not self.nNodes:
            self.nNodes = sum([t.getNNodes() for t in self.traces])/float(len(self.traces))
        return self.nNodes
        
    def getNAcks(self):
        '''
        Get average number of acks for all TraceRuns in this group.
        '''
        if not self.nAcks:
            self.nAcks = sum([t.getNAcks() for t in self.traces])/float(len(self.traces))
        return self.nAcks

    def getStdevNAcks(self):
        '''
        Standard deviation of the number of ACKs.
        '''
        if not self.stdevNAcks:
            #check for no acks at all...
            nacks = [t.getNAcks() for t in self.traces]
            if sum(nacks) == 0:
                self.stdevNAcks = 0
            else:
                self.stdevNAcks = scipy.stats.tstd(nacks)
        return self.stdevNAcks

    def getNDirectAcks(self):
        '''
        Get average number of direct acks for all TraceRuns in this group.
        '''
        if not self.nDirectAcks:
            self.nDirectAcks = sum([t.getNDirectAcks() for t in self.traces])/float(len(self.traces))
        return self.nDirectAcks

    def averageTimes(self, runs):
        '''
        Takes as input a list of TraceRun.getXTimes() outputs.  Merges the lists together so that the counts
        having the same time value are added, averages all of the counts, and then returns this new 2-tuple
        that matches the format of the TraceRun.getXTimes() funcions.
        '''
        newTimes = []
        newCounts = []
        nRuns = len(runs)
        nextIndex = nRuns*[0]
        pointsLeft = [len(run[0]) for run in runs]

        # Continually add together all counts that correspond to the same timestamp, building up the newTimes
        # and newCounts until all of the runs have been fully accounted for.
        while any(pointsLeft):
            nextTime = min([run[0][nextIndex[irun]] if pointsLeft[irun] else sys.maxint for irun,run in enumerate(runs)])
            newTimes.append(nextTime)
            timeAdded = False

            for irun,run in enumerate(runs):
                if pointsLeft[irun] and run[0][nextIndex[irun]] == nextTime:
                    if timeAdded:
                        newCounts[-1] += run[1][nextIndex[irun]]
                    else:
                        newCounts.append(run[1][nextIndex[irun]])
                        timeAdded = True

                    pointsLeft[irun] -= 1
                    nextIndex[irun] += 1

        # Average the elements
        nRuns = float(nRuns)
        return (newTimes, [count/nRuns for count in newCounts])

    def getSendTimes(self):
        '''
        Get average send times for all TraceRuns in this group. This will likely create a more smoothed curve.
        '''
        if not self.sendTimes:
            self.sendTimes = self.averageTimes([t.getSendTimes() for t in self.traces])
        return self.sendTimes

    def getAckTimes(self):
        '''
        Get average ack times for all TraceRuns in this group. This will likely create a more smoothed curve.
        '''
        if not self.ackTimes:
            self.ackTimes = self.averageTimes([t.getAckTimes() for t in self.traces])
        return self.ackTimes

    def getForwardTimes(self):
        '''
        Get average forward times for all TraceRuns in this group. This will likely create a more smoothed curve.
        '''
        if not self.forwardTimes:
            self.forwardTimes = self.averageTimes([t.getForwardTimes() for t in self.traces])
        return self.forwardTimes

    def getUtility(self):
        if not self.utility:
            utilities = [n.getUtility() for n in self.traces if n.getUtility() is not None]
            self.utility = float(sum(utilities))/len(utilities)
        return self.utility

################################# MAIN ####################################


if __name__ == '__main__':

    args = ParseArgs ()

    if not args.files and not args.dirs:
        print "You must specify some files or directories to parse!"
        exit(1)

    # First, deal with the globally affecting args
    if args.resolution:
        TraceRun.TIME_RESOLUTION = args.resolution

    # Build actual traces
    traceGroups = []
    if args.files:
        for f in args.files:
            traceGroups.append(TraceRun(f))

    elif args.dirs:
        for d in args.dirs:
            group = TraceGroup(d)
            if args.separate:
                traceGroups.extend(group.traces)
            else:
                traceGroups.append(group)
    
    # Fix label for the groups
    if args.label:   
        if (len(args.label) != len(traceGroups)) and (len(args.label) != 1):
            print "Number of given labels must equal the number of parsed directories or 1!"
            exit(1)

        if len(args.label) > 1:
            for i,g in enumerate(traceGroups):
                g.name = args.label[i]
        else:
            for g in traceGroups:
                g.name = args.label[0]

    if args.prepend_label:   
        if (len(args.prepend_label) != len(traceGroups)) and (len(args.prepend_label) != 1):
            print "Number of given appendix labels must equal the number of parsed directories or 1!"
            exit(1)

        if len(args.prepend_label) > 1:
            for i,g in enumerate(traceGroups):
                g.name = g.name + args.prepend_label[i]
        else:
            for g in traceGroups:
                g.name = g.name + args.prepend_label[0]

    if args.append_label:   
        if (len(args.append_label) != len(traceGroups)) and (len(args.append_label) != 1):
            print "Number of given appendix labels must equal the number of parsed directories or 1!"
            exit(1)

        if len(args.append_label) > 1:
            for i,g in enumerate(traceGroups):
                g.name += args.append_label[i]
        else:
            for g in traceGroups:
                g.name += args.append_label[0]


    ################################################################################
    #####################  Print any requested textual data analysis  ##############
    ################################################################################

    if args.summary:
        print "\n================================================= Summary =================================================\n"
        headings = ('Group name\t\t', 'Active Nodes', '# ACKs\t', '# Direct ACKs', '% Improvement', 'Utility\t', 'Stdev')
        print '%s\t'*len(headings) % headings, '\n'

        for g in traceGroups:
            nAcks = g.getNAcks()
            nDirectAcks = g.getNDirectAcks()
            improvement = percentImprovement(nAcks, nDirectAcks)
            utility = g.getUtility()
            try:
                stdev = g.getStdevNAcks()
            except AttributeError:
                stdev = 0.0
            print "\t\t".join(["%-20s", '%.2f', '%.2f', '%.2f', '%.2f', '%.2f', '%.2f']) % (g.name, g.getNNodes(), nAcks, nDirectAcks,
                                                                                    improvement, utility, stdev)
        print '\n==========================================================================================================='

    if args.t_test:
        print "\n================================================= T-Test ==================================================\n"
        print '%s\t'*6 % ('Group 1 name\t\t', 'Group 2 name\t\t', 'Group 1 mean', 'Group 2 mean', 't-statistic', 'p-value'), '\n'
        for i in range(0, len(traceGroups), 2):
            if i+1 >= len(traceGroups):
                print 'Not testing %s as uneven number of groups provided.' % traceGroups[i].name
                break

            g1 = traceGroups[i]
            g2 = traceGroups[i+1]
            
            if args.utility:
                print 'Using utility instead of # ACKs'
                g1_utilities = [r.getUtility() for r in g1.traces]
                g2_utilities = [r.getUtility() for r in g2.traces]
                
                (t_statistic, p_value) = scipy.stats.ttest_ind(g1_utilities, g2_utilities)
                print "\t\t".join(["%-20s", "%-20s", '%.2f', '%.2f', '%.2f', '%.2f']) % (g1.name, g2.name, g1.getUtility(), g2.getUtility(), t_statistic, p_value)

            else:
                g1_acks = [t.getNAcks() - t.getNDirectAcks() for t in g1.traces]
                g2_acks = [t.getNAcks() - t.getNDirectAcks() for t in g2.traces]

                (t_statistic, p_value) = scipy.stats.ttest_ind(g1_acks, g2_acks)
                print "\t\t".join(["%-20s", "%-20s", '%.2f', '%.2f', '%.2f', '%.2f']) % (g1.name, g2.name, g1.getNAcks() - g1.getNDirectAcks(), g2.getNAcks() - g2.getNDirectAcks(), t_statistic, p_value)

        print '\n==========================================================================================================='


#################################################################################################
###################################   PLOTS    ##################################################
#################################################################################################


    import matplotlib.pyplot as plt
    #import matplotlib.axes as ax

    def adjustXAxis(plot):
        '''
        Adjust the x-axis so that the left side is more visible.
        '''
        xmin, xmax = plot.xlim()
        adjusted = xmin - 0.05 * (xmax - xmin)
        plot.xlim(xmin=adjusted)

    nextTitleIdx = 0

    if args.time:
        markers = 'x.*+do^s1_|'
        for i,g in enumerate(traceGroups):
            plt.plot(*cumulative(normalizedTimes(g.getNNodes(), g.getAckTimes())), label=g.name, marker=markers[i%len(markers)])
        try:
            plt.title(" ".join(((args.prepend_title[nextTitleIdx if len(args.prepend_title) > 1 else 0]
                                 if args.prepend_title else ''),
                                (args.title[nextTitleIdx if len(args.title) > 1 else 0]
                                 if args.title else "Cumulative ACKs over time"),
                                (args.append_title[nextTitleIdx if len(args.append_title) > 1 else 0]
                                 if args.append_title else ''))))
        except IndexError:
            print "You must specify the same number of titles, prepend_titles, and append_titles, or else give "\
                "just 1 to apply to all plots (not all args must be specified, just make sure the ones you use match up)."
            exit(2)
        nextTitleIdx += 1
        plt.xlabel("Time (resolution = %ss)"% str(TraceRun.TIME_RESOLUTION))
        plt.ylabel("Normalized Count")
        plt.legend()
        adjustXAxis(plt)
        #ax.Axes.autoscale_view() #need instance

        if not args.no_windows:
            plt.show()

        '''if not args.no_save:
            savefig(os.path.join(args.output_directory, )'''

    if args.utility:
        markers = 'x.*+do^s1_|'
        # We first group the trace groups by their heuristic
        if isinstance(traceGroups[0], TraceGroup):
            runs = []
            for t in traceGroups:
                runs.extend(t.traces)
        else:
            runs = traceGroups

        def heuristic_key(run):
            return run.params.heuristic

        runs = sorted(runs, key=heuristic_key)
        groups = itertools.groupby(runs, heuristic_key)

        # Then order them by fprob and plot them
        def fprob_key(run):
            return run.params.fprob
        
        i = 0
        for heuristic,g in groups:
            #print [([r1.params.heuristic for r1 in r],heuristic) for heuristic,r in groups]

            fgroups = [g1 for g1 in g]
            fgroups = sorted(fgroups, key=fprob_key)
            fgroups = itertools.groupby(fgroups, fprob_key)

            ## build up fprobs and associated utilities for each
            utilities = []
            fprobs = []
            for fprob,fg in fgroups:
                fprobs.append(fprob)
                fruns = [frun for frun in fg] #extract runs
                utilities.append(sum([t.getUtility() for t in fruns])/len(fruns))

            plt.plot(fprobs, utilities, label=heuristic, marker=markers[i%len(markers)])

            i+=1

        try:
            plt.title(" ".join(((args.prepend_title[nextTitleIdx if len(args.prepend_title) > 1 else 0]
                                 if args.prepend_title else ''),
                                (args.title[nextTitleIdx if len(args.title) > 1 else 0]
                                 if args.title else "Heuristic Utility vs. Failure Probability"),
                                (args.append_title[nextTitleIdx if len(args.append_title) > 1 else 0]
                                 if args.append_title else ''))))
        except IndexError:
            print "You must specify the same number of titles, prepend_titles, and append_titles, or else give "\
                "just 1 to apply to all plots (not all args must be specified, just make sure the ones you use match up)."
            exit(2)
        nextTitleIdx += 1
        plt.xlabel("Failure probability")
        plt.ylabel("Average utility")
        plt.legend()
        adjustXAxis(plt)
        #ax.Axes.autoscale_view() #need instance

        if not args.no_windows:
            plt.show()

    if args.congestion:
        markers = 'x.*+do^s1_|'
        for i,g in enumerate(traceGroups):
            plt.plot(*normalizedTimes(g.getNNodes(), g.getSendTimes()), label=g.name, marker=markers[i%len(markers)])
        try:
            plt.title(" ".join(((args.prepend_title[nextTitleIdx if len(args.prepend_title) > 1 else 0]
                                 if args.prepend_title else ''),
                                (args.title[nextTitleIdx if len(args.title) > 1 else 0]
                                 if args.title else "Connection attempts over time"),
                                (args.append_title[nextTitleIdx if len(args.append_title) > 1 else 0]
                                 if args.append_title else ''))))
        except IndexError:
            print "You must specify the same number of titles, prepend_titles, and append_titles, or else give "\
                "just 1 to apply to all plots (not all args must be specified, just make sure the ones you use match up)."
            exit(2)
        plt.xlabel("Time (resolution = %ss)"% str(TraceRun.TIME_RESOLUTION))
        plt.ylabel("Normalized Count")
        plt.legend()
        adjustXAxis(plt)

        if not args.no_windows:
            plt.show()

    if args.improvement:
        colors = 'rgbycmkw'
        for i,g in enumerate(traceGroups):
            plt.bar(i, percentImprovement(g.getNAcks(), g.getNDirectAcks()), label=g.name, color=colors[i % len(colors)])
        try:
            plt.title(" ".join(((args.prepend_title[nextTitleIdx if len(args.prepend_title) > 1 else 0]
                                 if args.prepend_title else ''),
                                (args.title[nextTitleIdx if len(args.title) > 1 else 0]
                                 if args.title else "% improvement from overlay usage"),
                                (args.append_title[nextTitleIdx if len(args.append_title) > 1 else 0]
                                 if args.append_title else ''))))
        except IndexError:
            print "You must specify the same number of titles, prepend_titles, and append_titles, or else give "\
                "just 1 to apply to all plots (not all args must be specified, just make sure the ones you use match up)."
            exit(2)
        plt.xlabel("Groups")
        plt.ylabel("% Improvement")
        plt.legend()

        if not args.no_windows:
            plt.show()



        #subplot(nrows,ncols,next_axes)
    #try for smart subplot arrangements
    '''
    if nplots > 3:
        nrows = int(sqrt(nplots)+0.5)
        ncols = ceil(nplots/float(nrows))
    else:
        nrows = 1
        ncols = nplots

        if 'x' in arg:
            xvalues = gdata.get_xvalues(picks)
            subplot(nrows,ncols,next_axes)
            hist(xvalues,resolution)
            plt.title("Histogram of x-axis PGA readings")
            plt.xlabel("PGA (m/s^2)")
            plt.ylabel("Count")
            plt.plot()
            next_axes += 1

            hist([xvalues,yvalues,zvalues],resolution)
    '''
