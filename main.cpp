
struct EAPRTResult {
    double lastCompletedTime;
    Snapshot* newSnapshot;
};

class graph {
public:
    vector<Node*> nodes;
    vector<Node*> sortedNodes;

    void sortByRank() {
        Node* startNode;
        // find the start node of the graph
        for (Node* node : nodes) {
            if (node->parents.size() == 0) {
                startNode = node;
                break;
            }
        }
        sortNodes(startNode);

        // sort the nodes by rank
        quickSort(sortedNodes, 0, sortedNodes.size() - 1, [](Node* a, Node* b) {
            return a->rank < b->rank;
        });
    }

    double sortNodes(Node* node) {
        // if the node is exit node, the rank is its weight
        if (node->children.size() == 0) {
            node->rank = node->weight;
            return node->rank;
        } else {
            double maxRank = 0;
            for (Node* child : node->children) {
                maxRank = max(maxRank, node->communicationCost + sortNodes(child));
            }
            return maxRank;
        }
    }

    Node* pop() {
        if (sortedNodes.size() == 0) {
            return nullptr;
        } else {
            Node* node = sortedNodes[0];
            sortedNodes.erase(sortedNodes.begin());
            return node;
        }
    }

    Snapshot snapshotNow() {
        Snapshot snapshot;
        for (Node* node : nodes) {
            snapshot[node] = node->CRT;
        }
        return snapshot;
    }

    void restoreSnapshot(Snapshot snapshot) {
        for (Node* node : nodes) {
            node->CRT = snapshot[node];
        }
    }
};





class Node {
public:
    double weight;
    double communicationCost;
    double CRT;
    Device* device;
    Graph* graph;
    vector<Node*> parents;
    vector<Node*> children;
    Channels* channels;
    int rank;

    void setCRT(double CRT) {
        this->CRT = CRT;
    }

    double getCRT() {
        return CRT;
    }
    
    void adjustCRT(double outfeedStartTime) {
        channels->adjustOutfeed(outfeedStartTime, communicationCost);
        CRT = max(CRT, channel->getAvailableOutFeed());
    }

    EAPRTResult getEAPRT(Device* device) {
        // record a snapshot of the current assignment of nodes to devices
        Snapshot* originalSnapshot = graph->snapshotNow();
        for (Node* parent : parents) {
            if (parent->device == device) {
                continue;
            } else {
                // put the parent node into the infeed of this device according to the communication cost and its CRT, 
                // and return the actual outfeed start time
                double outfeedStartTime = device->putIntoInfeed(parent->communicationCost, parent->getCRT());

                // adjust the CRT of the parent node, beacuse of the change in its outfeed channel
                parent->adjustCRT(outfeedStartTime);
            }
        }
        // record a new snapshot of the assignment of nodes to devices
        Snapshot* newSnapshot = graph->snapshotNow();
        double lastCompletedTime = 0;
        for (Channel* infeed : channels->getInfeedChannels()) {
            lastCompletedTime = max(lastCompletedTime, infeed->getLastCompletedTime(parents));
        }
        // turn the graph back to the state before the EAPRT calculation
        graph->restoreSnapshot(originalSnapshot);
        // return the last completed time and the new snapshot
        return EAPRTResult{lastCompletedTime, newSnapshot};
    }
};


int main() {

    graph.sortByRank();

    Node* node = graph.pop();
    while (node != nullptr) {
        double largestERT = 0;
        Device* largestERTDevice = nullptr;
        Snapshot snapShotOfLargeERTAssignment;

        // iterate the ERT of every device, find the device with the largest ERT
        for (Device* device : allDevices) {
            EAPRTResult EAPRTResult = node->getEAPRT(device);
            double EST = max(device->availableKernel, EAPRTResult.lastCompletedTime);
            double ERT = max(device->availableOutfeed, node->weight + EST);
            if (ERT > largestERT) {
                largestERT = ERT;
                largestERTDevice = device;
                snapShotOfLargeERTAssignment = EAPRTResult.newSnapshot;
            }
        }
        // Assign the largest ERT to the node ot make it CRT
        node->CRT = largestERT;
        // Assign the node to the device with the largest ERT
        node->device = largestERTDevice;
        // restore the snapshot of the assignment of nodes to devices
        node->graph->restoreSnapshot(snapShotOfLargeERTAssignment);
        // get the next node
        node = graph.pop();
    }

    Node exitNode = graph.getExitNode();

    return exitNode.getCRT();


}