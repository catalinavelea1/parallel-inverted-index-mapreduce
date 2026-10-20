#include <iostream>
#include <vector>
#include <fstream>
#include <set>
#include <string>
#include <map>
#include <algorithm>
#include <pthread.h>
#include <queue>
#include <unordered_map>
#include <array>

struct GlobalData {
    int M;
    int R;
    int N;

    std::vector<std::vector<std::pair<std::string, int>>> partial_lists;
    std::queue<std::pair<std::string, int>> file_queue;
    std::unordered_map<std::string, std::set<int>> words_map;
    std::array<std::vector<std::pair<std::string, std::vector<int>>>, 26> sorted_words;

    pthread_mutex_t mutex;
    pthread_barrier_t barrier;
    pthread_barrier_t map_barrier;
};

void *map_files(void *arg) {
    auto *data = static_cast<std::pair<int, GlobalData*>*>(arg);
    int id = data->first;
    GlobalData* shared = data->second;

    std::vector<std::pair<std::string, int>> thread_list;

    while (true) {
        int file_index = -1;
        std::string filename;

        pthread_mutex_lock(&shared->mutex);
        if (!shared->file_queue.empty()) {
            auto current_file = shared->file_queue.front();
            file_index = current_file.second;
            filename = current_file.first;
            shared->file_queue.pop();
        } else {
            pthread_mutex_unlock(&shared->mutex);
            break;
        }
        pthread_mutex_unlock(&shared->mutex);

        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Unable to open input file!\n";
            exit(1);
        }

        std::string buffer;
        std::string word;
        while (file >> buffer) {
            for (int letter : buffer) {
                if (letter >= 65 && letter <= 90) {
                    letter += 32;
                    word.push_back((char)letter);
                } else if (letter >= 97 && letter <= 122) {
                    word.push_back((char)letter);
                }
            }

            if (!word.empty()) {
                thread_list.push_back({word, file_index});
                word.clear();
            }    
        }

        file.close();
    }

    shared->partial_lists[id] = std::move(thread_list);
    pthread_barrier_wait(&shared->map_barrier);
    pthread_exit(NULL);
}

void *reduce_files(void *arg) {
    auto *data = static_cast<std::pair<int, GlobalData*>*>(arg);
    int id = data->first;
    GlobalData* shared = data->second;

    pthread_barrier_wait(&shared->map_barrier);

    int start = id * (double)shared->M / shared->R;
    int end = (id + 1) * (double)shared->M / shared->R;

    std::unordered_map<std::string, std::set<int>> local_map;

    for (int i = start; i < end; i++) {
        for (const auto& p : shared->partial_lists[i]) {
            local_map[p.first].insert(p.second);
        }
    }

    pthread_mutex_lock(&shared->mutex);
    for (const auto& [word, indexes] : local_map) {
        shared->words_map[word].insert(indexes.begin(), indexes.end());
    }
    pthread_mutex_unlock(&shared->mutex);

    pthread_barrier_wait(&shared->barrier);

    if (id == 0) {
        for (const auto& [word, file_indexes] : shared->words_map) {
            char first_letter = word[0];
            int i = first_letter - 'a';
            shared->sorted_words[i].emplace_back(word, std::vector<int>(file_indexes.begin(), file_indexes.end()));
        }
    }

    pthread_barrier_wait(&shared->barrier);

    start = id * (double)26 / shared->R;
    end = (id + 1) * (double)26 / shared->R;

    for (int i = start; i < end; i++) {
        std::sort(shared->sorted_words[i].begin(), shared->sorted_words[i].end(), [](const auto& a, const auto& b) {
            if (a.second.size() != b.second.size()) {
                return a.second.size() > b.second.size();
            }
            return a.first < b.first;
        });
        
        std::string filename(1, i + 97);
        filename.append(".txt");

        std::ofstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Unable to open file: " << filename << "\n";
            exit(1);
        }

        for (const auto &pair : shared->sorted_words[i]) {
            file << pair.first << ":" << "[";
            bool first = true;
            for (int file_index : pair.second) {
                if (!first) {
                    file << " ";
                }
                file << file_index + 1;
                first = false;
            }
            file << "]\n"; 
        }

        file.close();
        if (file.fail()) {
            std::cerr << "Error closing file: " << filename << "\n";
            exit(1);
        }
    }

    pthread_exit(NULL);
}

void get_files(const std::string& input_file, GlobalData &shared) {
    std::ifstream file(input_file);
    if (!file.is_open()) {
        std::cerr << "Unable to open input file!\n";
        exit(1);
    }

    file >> shared.N;

    for (int i = 0; i < shared.N; ++i) {
        std::string filename;
        file >> filename;
        shared.file_queue.push({filename, i});
    }

    file.close();
    if (file.fail()) {
        std::cerr << "Unable to close the file!\n";
        exit(1);
    }
}

int main(int argc, char** argv) {
    if (argc < 4) {
        std::cerr << "Not enough arguments!\n";
        exit(1);
    }

    GlobalData shared;
    shared.M = atoi(argv[1]);
    shared.R = atoi(argv[2]);

    get_files(argv[3], shared);

    shared.partial_lists.resize(shared.M);
    pthread_mutex_init(&shared.mutex, NULL);
    pthread_barrier_init(&shared.barrier, NULL, shared.R);
    pthread_barrier_init(&shared.map_barrier, NULL, shared.M + shared.R);

    pthread_t threads[shared.M + shared.R];
    std::pair<int, GlobalData*> thread_args[shared.M + shared.R];

    for (int i = 0; i < shared.M + shared.R; i++) {
        if (i < shared.M) {
            thread_args[i].first = i;
            thread_args[i].second = &shared;
            pthread_create(&threads[i], NULL, map_files, &thread_args[i]);
        } else {
            thread_args[i].first = i - shared.M;
            thread_args[i].second = &shared;
            pthread_create(&threads[i], NULL, reduce_files, &thread_args[i]);
        }
    }

    for (int i = 0; i < shared.M + shared.R; i++) {
        pthread_join(threads[i], NULL);
    }

    pthread_mutex_destroy(&shared.mutex);
    pthread_barrier_destroy(&shared.barrier);
    pthread_barrier_destroy(&shared.map_barrier);

    return 0;
}
