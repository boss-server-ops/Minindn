#ifndef K_MEANS_H_
#define K_MEANS_H_

#include <random>
#include <vector>
#include <map>
#include <string>

class KMeans {
public:
    enum InitMethod { kForgy, kRandomPartition };
    KMeans(const std::vector<std::string>& data, int k,
           InitMethod init_method, unsigned int seed);
    const std::vector<std::vector<std::string>>& cluster_centers() const;
    const std::vector<int>& assignments() const;
    double GetSumSquaredError(const std::map<std::string, std::map<std::string, int>> linkCostMatrix) const;
    std::vector<std::vector<std::string>> clusters;

protected:
    void Init();
    int CalDistance(const std::string& data1,
                    const std::string& data2,
                    std::map<std::string, std::map<std::string, int>> linkCostMatrix) const;
    int CalDistance(const std::string& data1,
                    const std::vector<std::string>& clu,
                    std::map<std::string, std::map<std::string, int>> linkCostMatrix) const;
    void UpdateClusterCenter();
    void InitWithRandomCenter();
    void InitWithRandomAssignment();
    const int n_;
    // const int s_;
    const int k_;
    const std::vector<std::string> data_;
    InitMethod init_method_;
    const unsigned int seed_;
    std::vector<int> assignments_;
    std::default_random_engine el_;
};

#endif  // K_MEANS_H_
