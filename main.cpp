
struct EAPRTResult {
    double lastCompletedTime;
    Snapshot newSnapshot;
};

class Node {
public:
    double weight;
    double communicationCost;
    double CRT;
    Device* device;
    Graph* graph;
    vector<Node*> parents;
    Channels* channels;

    void setCRT(double CRT) {
        this->CRT = CRT;
    }

    void setDevice(Device device) {
        this->device = device;
    }

    double getCRT() {
        return CRT;
    }
    
    void adjustCRT(double outfeedStartTime) {
        channels->adjustOutfeed(outfeedStartTime, communicationCost);
        CRT = max(CRT, channel->getAvailableOutFeed());
    }

    EAPRTResult getEAPRT(Device* device) {
        Snapshot originalSnapshot = graph->snapshotNow();
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
        Snapshot newSnapshot = graph->snapshotNow();
        double lastCompletedTime = 0;
        for (Channel* infeed : channels->getInfeedChannels()) {
            lastCompletedTime = max(lastCompletedTime, infeed->getLastCompletedTime(parents));
        }
        graph->restoreSnapshot(originalSnapshot);
        return EAPRTResult{lastCompletedTime, newSnapshot};
    }


};


int main() {

    graph.sortByRank();

    Node* node = graph.pop();
    while (node != nullptr) {
        double largestERT = 0;
        Device* largestERTDevice = nullptr;
        for (Device* device : allDevices) {
            double EAPRT = node->getEAPRT(device).lastCompletedTime;
            double EST = max(device->availableKernel, EAPRT);
            double ERT = max(device->availableOutfeed, node->weight + EST);
            if (ERT > largestERT) {
                largestERT = ERT;
                largestERTDevice = device;
            }
        }
        node->setCRT(largestERT);
        node->setDevice(largestERTDevice);
        node = graph.pop();
    }

    Node exitNode = graph.getExitNode();

    return exitNode.getCRT();


}