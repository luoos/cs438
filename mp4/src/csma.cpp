#include <iostream>
#include <numeric>
#include <fstream>
#include <sstream>
#include <vector>

using namespace std;

class Node {
    private:
    int curCollisions_;  // collision for current packet
    int maxRetryCnt_;
    int *RArray_;

    public:
    int countDown_;
    int totalCollisions_, successTrans_;

    Node(int maxRetryCnt, int *RArray) {
        totalCollisions_ = 0;
        successTrans_ = 0;
        curCollisions_ = 0;
        maxRetryCnt_ = maxRetryCnt;
        RArray_ = RArray;
        resetCountDown();
    }

    void resetCountDown() {
        int r = RArray_[curCollisions_];
        countDown_ = rand() % (r + 1);
    }

    void collide() {
        totalCollisions_ += 1;
        curCollisions_ = (curCollisions_ + 1) % maxRetryCnt_;
        resetCountDown();
    }

    void successTrans() {
        successTrans_ += 1;
        curCollisions_ = 0;
        resetCountDown();
    }
};

void checkReadyNodes(vector<Node*> *readyNodes, Node* *nodes, int nodeCnt) {
    readyNodes->clear();
    for (int i = 0; i < nodeCnt; i++) {
        if (nodes[i]->countDown_ == 0) {
            readyNodes->push_back(nodes[i]);
        }
    }
}

int countDownAll(Node* *nodes, int nodeCnt) {
    int minCountDown = 191919191;
    for (int i = 0; i < nodeCnt; i++) {
        if (nodes[i]->countDown_ < minCountDown) {
            minCountDown = nodes[i]->countDown_;
        }
    }
    for (int i = 0; i < nodeCnt; i++) {
        nodes[i]->countDown_ -= minCountDown;
    }
    return minCountDown;
}

vector<double> getAllSuccessTransCnt(Node* *nodes, int nodeCnt) {
    vector<double> arr;
    for (int i = 0; i < nodeCnt; i++) {
        arr.push_back((double) nodes[i]->successTrans_);
    }
    return arr;
}

vector<double> getAllCollisionsCnt(Node* *nodes, int nodeCnt) {
    vector<double> arr;
    for (int i = 0; i < nodeCnt; i++) {
        arr.push_back((double) nodes[i]->totalCollisions_);
    }
    return arr;
}

double computeVariance(vector<double> arr) {
    double mean = accumulate(arr.begin(), arr.end(), 0.0) / arr.size();
    double sum = 0.0;
    for (double n : arr) {
        sum += (n - mean) * (n - mean);
    }
    return sum / (arr.size() - 1);
}

void simulate(int n, int l, int *r, int m, int maxTime, vector<double> *channelUtilVec,
              vector<double> *channelIdleVec, vector<double> *collisionCntVec,
              vector<double> *successTransVarianceVec, vector<double> *collisionVarianceVec) {
    srand( time( NULL ) );

    Node* nodes[n];
    for (int i = 0; i < n; i++) {
        nodes[i] = new Node(m, r);
    }

    // Nodes count down to 0 and ready to send.
    // if there is no node ready, the channel is idle
    // if there is only one node ready, it transmit successfully, the channel is busy
    // if there is more than one node ready, they collide.
    vector<Node*> nodesReady;

    int curTime = 0;
    int idleTime = 0, busyTime = 0, collisionCnt = 0;
    while (curTime < maxTime) {
        checkReadyNodes(&nodesReady, nodes, n);
        if (nodesReady.size() == 0) {
            // idle
            int minCountDown = countDownAll(nodes, n);
            curTime += minCountDown;  // time elapsed during count down
            idleTime += minCountDown;
        } else if (nodesReady.size() == 1) {
            // busy
            nodesReady.at(0)->successTrans();
            curTime += l;  // time to send the packet
            busyTime += l;
        } else {
            // collision
            for (Node* n : nodesReady) {
                n->collide();
            }
            curTime += 1;
            collisionCnt += 1;
            busyTime += 1;
        }
    }
    double channelBusyPercentage = busyTime / (double) curTime;
    double channelIdlePercentage = idleTime / (double) curTime;
    double varianceOfSuccessTrans = computeVariance(getAllSuccessTransCnt(nodes, n));
    double varianceOfCollisions = computeVariance(getAllCollisionsCnt(nodes, n));
    channelUtilVec->push_back(channelBusyPercentage);
    channelIdleVec->push_back(channelIdlePercentage);
    collisionCntVec->push_back((double) collisionCnt);
    successTransVarianceVec->push_back(varianceOfSuccessTrans);
    collisionVarianceVec->push_back(varianceOfCollisions);

    // free created nodes
    for (Node *n : nodes) {
        free(n);
    }
}

void parseInputFile(string inputFileName,
        int *N, int *L, int **R, int *M, int *T) {
    ifstream inputFile(inputFileName);
    string line;
    // parse N
    getline(inputFile, line);
    line = line.substr(2);
    *N = stoi(line);

    // parse L
    getline(inputFile, line);
    line = line.substr(2);
    *L = stoi(line);

    // parse R
    getline(inputFile, line);
    line = line.substr(2);
    vector<int> R_vec;
    istringstream iss(line);
    for (string s; iss >> s; ) {
        R_vec.push_back(stoi(s));
    }
    *R = new int[R_vec.size()];
    copy(R_vec.begin(), R_vec.end(), *R);

    // parse M
    getline(inputFile, line);
    line = line.substr(2);
    *M = stoi(line);

    // parse T
    getline(inputFile, line);
    line = line.substr(2);
    *T = stoi(line);
}

double getMean(vector<double> *arr) {
    return accumulate(arr->begin(), arr->end(), 0.0) / arr->size();
}

void writeOutputFile(double channelUtilAvg, double channelIdleAvg,
                     double numOfCollisionAvg,
                     double varianceOfSuccessTransAvg, double varianceOfCollisionAvg) {
    ofstream file("output.txt");
    file << "Channel utilization (in percentage) " << channelUtilAvg << endl;
    file << "Channel idle fraction (in percentage) " << channelIdleAvg << endl;
    file << "Total number of collisions " << (int) numOfCollisionAvg << endl;
    file << "Variance in number of successful transmissions (across all nodes) "
         << varianceOfSuccessTransAvg << endl;
    file << "Variance in number of collisions (across all nodes) "
         << varianceOfCollisionAvg << endl;
    file.close();
}

int main(int argc, char** argv) {
    if (argc != 2) {
        cout << "Usage: ./csma input.txt" << endl;
        return -1;
    }
    int N, L, *R, M, T;  // config by input.txt
    parseInputFile(argv[1], &N, &L, &R, &M, &T);
    vector<double> channelUtilVec, channelIdleVec, collisionCntVec;
    vector<double> successTransVarianceVec, collisionVarianceVec;
    for (int i = 0; i < 200; i++) {
        simulate(N, L, R, M, T, &channelUtilVec, &channelIdleVec, &collisionCntVec,
                 &successTransVarianceVec, &collisionVarianceVec);
    }
    free(R);

    double channelUtilAvg = getMean(&channelUtilVec);
    double channelIdleAvg = getMean(&channelIdleVec);
    double numOfCollisionAvg = getMean(&collisionCntVec);
    double varianceOfSuccessTransAvg = getMean(&successTransVarianceVec);
    double varianceOfCollisionAvg = getMean(&collisionVarianceVec);

    cout << channelUtilAvg << endl << channelIdleAvg << endl << numOfCollisionAvg << endl;
    cout << varianceOfSuccessTransAvg << endl << varianceOfCollisionAvg << endl;

    writeOutputFile(channelUtilAvg, channelIdleAvg, numOfCollisionAvg,
                    varianceOfSuccessTransAvg, varianceOfCollisionAvg);

    return 0;
}
