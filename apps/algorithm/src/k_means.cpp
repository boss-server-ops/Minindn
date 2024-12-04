#include "../include/k_means.hpp"

// Pseudo-random number generation
std::random_device rd;

KMeans::KMeans(const std::vector<std::string>& data, int k,
               InitMethod init_method, unsigned int seed)
        : data_(data),
          n_(static_cast<int>(data.size())),
        // Do not have feature in each data point.
        // s_(static_cast<int>(data.front().size())),
          k_(k),
          clusters(std::vector<std::vector<std::string>>(k)), // initialization of clusters
          init_method_(init_method),
          el_(seed),
          seed_(seed) {}

const std::vector<std::vector<std::string>>& KMeans::cluster_centers() const {
    return this->clusters;
}

const std::vector<int>& KMeans::assignments() const {
    return this->assignments_;
}

// todo: Done!
int KMeans::CalDistance(const std::string& data1,
                        const std::string& data2,
                        std::map<std::string, std::map<std::string, int>> linkCostMatrix) const {
    return linkCostMatrix[data1][data2];
}

// todo: Done!
int KMeans::CalDistance(const std::string& data1,
                        const std::vector<std::string>& cluster,
                        std::map<std::string, std::map<std::string, int>> linkCostMatrix) const {
    int distance = 0;
    for (const auto& node : cluster) {
        distance += linkCostMatrix[node][data1];
    }
    distance = distance / cluster.size();
    return distance;
}

void KMeans::Init() {
    InitWithRandomAssignment();
    switch (init_method_) {
        case kForgy:
            InitWithRandomCenter();
            break;
        case kRandomPartition:
            UpdateClusterCenter();
            break;
    }
}

// update the clusters according to the new results assignment
void KMeans::UpdateClusterCenter() {
    clusters.clear();
    clusters.resize(k_);
    for (int i = 0; i < n_; ++i) {
        clusters[assignments_[i]].emplace_back(data_[i]);
    }
}

double KMeans::GetSumSquaredError(const std::map<std::string, std::map<std::string, int>> linkCostMatrix) const {
    double sum = 0;
    for (int i = 0; i < n_; ++i) {
        // ToDo: change the code
        sum += CalDistance(data_[i], clusters[assignments_[i]], linkCostMatrix);
    }
    return sum;
}

// Use the result of assignment, assign the data to the clusters
void KMeans::InitWithRandomCenter() {
    clusters.resize(k_);
    for (int i = 0; i < n_; ++i) {
        clusters[assignments_[i]].emplace_back(data_[i]);
    }
}

void KMeans::InitWithRandomAssignment() {
    assignments_.resize(n_);
    std::uniform_int_distribution<int> dist(0, k_ - 1);
    for (int i = 0; i < n_; ++i) {
        assignments_[i] = dist(el_);
    }
}
