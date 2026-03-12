#include <iostream>
#include <vector>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <chrono>
#include <string>

using namespace std;

/**
 * @struct Instance
 * @brief Stores one Taillard scheduling instance.
 *
 * This structure holds the file name, the number of jobs,
 * the number of machines, and the processing-time matrix.
 *
 * The matrix p is indexed as p[j][m], where:
 * - j is the job index
 * - m is the machine index
 */
struct Instance {
    string name;
    int jobs;
    int machines;
    vector<vector<int>> p;
};

/**
 * @brief Loads a Taillard problem instance from a text file.
 *
 * This function reads all integers from the input file, interprets
 * the first integer as the number of jobs and the second integer as
 * the number of machines, and then reads the final jobs * machines
 * integers as the processing-time matrix.
 *
 * @param filename The path to the input file.
 * @param inst Reference to the Instance object that will be filled.
 * @return true if the file was opened and parsed successfully.
 * @return false if the file could not be opened or did not contain enough data.
 */
bool loadInstance(const string& filename, Instance& inst) {
    ifstream file(filename);

    if (!file.is_open()) {
        return false;
    }

    vector<int> nums;
    int x;

    while (file >> x) {
        nums.push_back(x);
    }

    if (nums.size() < 2) {
        return false;
    }

    inst.jobs = nums[0];
    inst.machines = nums[1];

    if ((int)nums.size() < 2 + inst.jobs * inst.machines) {
        return false;
    }

    inst.p.assign(inst.jobs, vector<int>(inst.machines, 0));

    int startIndex = (int)nums.size() - (inst.jobs * inst.machines);
    int idx = startIndex;

    for (int j = 0; j < inst.jobs; j++) {
        for (int m = 0; m < inst.machines; m++) {
            inst.p[j][m] = nums[idx++];
        }
    }

    return true;
}

/**
 * @brief Computes the makespan for a standard flow shop schedule.
 *
 * This function evaluates a sequence of jobs under the standard
 * Flow Shop Scheduling (FSS) model, where jobs move to the next
 * machine as soon as both the job and machine are ready.
 *
 * @param inst The problem instance containing the processing-time matrix.
 * @param seq The job sequence to evaluate.
 * @return The makespan of the provided sequence.
 */
int makespanFSS(const Instance& inst, const vector<int>& seq) {
    if (seq.empty()) {
        return 0;
    }

    vector<vector<int>> c((int)seq.size(), vector<int>(inst.machines, 0));

    for (int i = 0; i < (int)seq.size(); i++) {
        int job = seq[i];

        for (int m = 0; m < inst.machines; m++) {
            int prevJob = (i == 0) ? 0 : c[i - 1][m];
            int prevMachine = (m == 0) ? 0 : c[i][m - 1];

            c[i][m] = max(prevJob, prevMachine) + inst.p[job][m];
        }
    }

    return c[(int)seq.size() - 1][inst.machines - 1];
}

/**
 * @brief Computes the makespan for a blocking flow shop schedule.
 *
 * This function evaluates a sequence of jobs under the Flow Shop
 * Scheduling with Blocking (FSSB) model. A simplified blocking rule
 * is used so that if the next machine is still occupied, the current
 * job may be delayed on its current machine.
 *
 * @param inst The problem instance containing the processing-time matrix.
 * @param seq The job sequence to evaluate.
 * @return The makespan of the provided sequence under blocking constraints.
 */
int makespanFSSB(const Instance& inst, const vector<int>& seq) {
    if (seq.empty()) {
        return 0;
    }

    vector<vector<int>> c((int)seq.size(), vector<int>(inst.machines, 0));

    for (int i = 0; i < (int)seq.size(); i++) {
        int job = seq[i];

        for (int m = 0; m < inst.machines; m++) {
            int prevJob = (i == 0) ? 0 : c[i - 1][m];
            int prevMachine = (m == 0) ? 0 : c[i][m - 1];

            int start = max(prevJob, prevMachine);
            int finish = start + inst.p[job][m];

            // simplified blocking rule
            if (m < inst.machines - 1 && i > 0) {
                finish = max(finish, c[i - 1][m + 1]);
            }

            c[i][m] = finish;
        }
    }

    return c[(int)seq.size() - 1][inst.machines - 1];
}

/**
 * @brief Generates a schedule using the NEH heuristic.
 *
 * This function computes the total processing time for each job,
 * sorts the jobs in decreasing order of total processing time,
 * and then incrementally builds the schedule by inserting each
 * job into the position that produces the best makespan.
 *
 * If blocking is false, the standard FSS makespan is used.
 * If blocking is true, the FSSB makespan is used.
 *
 * @param inst The problem instance containing jobs, machines, and processing times.
 * @param blocking Flag indicating whether blocking constraints should be used.
 * @return A vector containing the NEH-generated job sequence.
 */
vector<int> neh(const Instance& inst, bool blocking) {
    vector<pair<int, int>> totals;

    for (int j = 0; j < inst.jobs; j++) {
        int sum = 0;
        for (int m = 0; m < inst.machines; m++) {
            sum += inst.p[j][m];
        }

        totals.push_back({ -sum, j });
    }

    sort(totals.begin(), totals.end());

    vector<int> seq;

    for (auto& item : totals) {
        int job = item.second;

        if (seq.empty()) {
            seq.push_back(job);
            continue;
        }

        vector<int> bestSeq;
        int bestMakespan = 1000000000;

        for (int pos = 0; pos <= (int)seq.size(); pos++) {
            vector<int> testSeq = seq;
            testSeq.insert(testSeq.begin() + pos, job);

            int span;
            if (blocking) {
                span = makespanFSSB(inst, testSeq);
            }
            else {
                span = makespanFSS(inst, testSeq);
            }

            if (span < bestMakespan) {
                bestMakespan = span;
                bestSeq = testSeq;
            }
        }

        seq = bestSeq;
    }

    return seq;
}

/**
 * @brief Converts a job sequence vector into a space-separated string.
 *
 * This is used when writing job sequences to the CSV output file.
 *
 * @param seq The sequence of jobs.
 * @return A string containing the sequence values separated by spaces.
 */
string seqToString(const vector<int>& seq) {
    stringstream ss;

    for (int i = 0; i < (int)seq.size(); i++) {
        ss << seq[i];
        if (i < (int)seq.size() - 1) {
            ss << " ";
        }
    }

    return ss.str();
}

/**
 * @brief Main driver for the project.
 *
 * This function:
 * - Opens the CSV output files
 * - Iterates through Taillard instance files 1.txt to 120.txt
 * - Loads each instance
 * - Runs NEH for both FSS and FSSB
 * - Measures execution time for each variant
 * - Writes the makespan and timing results to raw_results.csv
 * - Writes the generated schedules to raw_schedules.csv
 *
 * @return 0 if the program completes successfully, or 1 if an output file could not be created.
 */
int main() {
    ofstream results("raw_results.csv");
    ofstream schedules("raw_schedules.csv");

    if (!results.is_open()) {
        std::cout << "Could not create raw_results.csv\n";
        return 1;
    }

    if (!schedules.is_open()) {
        std::cout << "Could not create raw_schedules.csv\n";
        return 1;
    }

    results << "instance,type,jobs,machines,makespan,time_ms\n";
    schedules << "instance,type,sequence\n";

    string folder = "Taillard_TestData/";

    for (int fileNumber = 1; fileNumber <= 120; fileNumber++) {
        string filename = folder + to_string(fileNumber) + ".txt";

        Instance inst;
        inst.name = filename;

        if (!loadInstance(filename, inst)) {
            std::cout << "Could not load " << filename << "\n";
            continue;
        }

        // FSS
        auto startFSS = chrono::high_resolution_clock::now();
        vector<int> seqFSS = neh(inst, false);
        auto endFSS = chrono::high_resolution_clock::now();

        double timeFSS = chrono::duration<double, milli>(endFSS - startFSS).count();
        int makespanFss = makespanFSS(inst, seqFSS);

        results << filename << ",FSS,"
            << inst.jobs << ","
            << inst.machines << ","
            << makespanFss << ","
            << timeFSS << "\n";

        schedules << filename << ",FSS,\""
            << seqToString(seqFSS) << "\"\n";

        // FSSB
        auto startFSSB = chrono::high_resolution_clock::now();
        vector<int> seqFSSB = neh(inst, true);
        auto endFSSB = chrono::high_resolution_clock::now();

        double timeFSSB = chrono::duration<double, milli>(endFSSB - startFSSB).count();
        int makespanFssb = makespanFSSB(inst, seqFSSB);

        results << filename << ",FSSB,"
            << inst.jobs << ","
            << inst.machines << ","
            << makespanFssb << ","
            << timeFSSB << "\n";

        schedules << filename << ",FSSB,\""
            << seqToString(seqFSSB) << "\"\n";

        std::cout << "Finished " << filename << "\n";
    }

    results.close();
    schedules.close();

    std::cout << "All 120 files processed.\n";
    return 0;
}