#include <iostream>
#include <filesystem>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <algorithm>
#include <boost/filesystem.hpp>
#include <fstream>
#include <vector>
#include "client_http.hpp"
#include "server_http.hpp"
#include "json.hh"

using json = nlohmann::json;

using namespace std;
using namespace boost::property_tree;
using HttpServer = SimpleWeb::Server<SimpleWeb::HTTP>;
using HttpClient = SimpleWeb::Client<SimpleWeb::HTTP>;

HttpClient seed("localhost:8080");
std::string profile_json_string;


struct Node {
    string ip;
    int port;
    string name;

    bool operator<(const Node& other) const {
        // You can define the comparison logic based on your requirements.
        // For example, you can compare based on IP, port, or name.
        if (ip != other.ip) {
            return ip < other.ip;
        }
        if (port != other.port) {
            return port < other.port;
        }
        return name < other.name;
    }
};

void sigint_handler(int signo) {
    printf("Received SIGINT (Ctrl+C)\n");
    auto offline = seed.request("POST", "/doOfflineNode",profile_json_string);
    cout << offline->content.rdbuf() << endl;
    exit(0);
}

vector<Node> get_nodes(){
  auto r1 = seed.request("GET", "/onlineNodes", profile_json_string);
  json jsonArray = json::parse(r1->content.string());
  std::vector<Node> nodes;

    for (const auto& item : jsonArray) {
        Node node;
        node.ip = item["ip"].get<std::string>();
        node.port = item["port"].get<int>();
        node.name = item["name"].get<std::string>();
        nodes.push_back(node);
    }
  return nodes;
}

map<Node,vector<string>> get_files(vector<Node> nodes, int port){
  map<Node,vector<string>> files;
  int count = 1;
  for (const Node& value : nodes) {
        if(value.port != port) {
            string connectionString = value.ip + ":" + to_string(value.port);
            HttpClient peer(connectionString);
            auto r3 = peer.request("GET", "/files");
            json jsonArray = json::parse(r3->content.string());
            std::vector<string> name_of_files = jsonArray.get<std::vector<string>>();
            for(string &name : name_of_files) {
                name = to_string(count++) + ". " + name;
            }
            files.insert({value,name_of_files});
        }
  }

  return files;
}


int main(){
    if (signal(SIGINT, sigint_handler) == SIG_ERR) {
        perror("Could not set SIGINT handler");
        return 1;
    }
    HttpServer server;
    namespace fs = std::filesystem;

    server.resource["^/files"]["GET"] = [](shared_ptr<HttpServer::Response> response, shared_ptr<HttpServer::Request> request) {
        fs::path currentDirectory = fs::current_path();
        json jsonArray = json::array();
        for (const auto& entry : fs::directory_iterator(currentDirectory)) {
            if (fs::is_regular_file(entry)) {
                jsonArray.push_back(entry.path().filename());
            }
        }
        string jsonString = jsonArray.dump();
        response->write(jsonString);
    };

    server.resource["^/download"]["GET"] = [](shared_ptr<HttpServer::Response> response, shared_ptr<HttpServer::Request> request) {
              
        SimpleWeb::CaseInsensitiveMultimap header;
        auto ifs = make_shared<ifstream>();
        string path =  fs::current_path().string() + "/" + request->content.string();
        ifs->open(path, ifstream::in | ios::binary | ios::ate);
        if(*ifs) {
        auto length = ifs->tellg();
        ifs->seekg(0, ios::beg);

        header.emplace("Content-Length", to_string(length));
        response->write(header);

        // Trick to define a recursive function within this scope (for example purposes)
        class FileServer {
        public:
          static void read_and_send(const shared_ptr<HttpServer::Response> &response, const shared_ptr<ifstream> &ifs) {
            // Read and send 128 KB at a time
            static vector<char> buffer(131072); // Safe when server is running on one thread
            streamsize read_length;
            if((read_length = ifs->read(&buffer[0], static_cast<streamsize>(buffer.size())).gcount()) > 0) {
              response->write(&buffer[0], read_length);
              if(read_length == static_cast<streamsize>(buffer.size())) {
                response->send([response, ifs](const SimpleWeb::error_code &ec) {
                  if(!ec)
                    read_and_send(response, ifs);
                  else
                    cerr << "Connection interrupted" << endl;
                });
              }
            }
          }
        };
        FileServer::read_and_send(response, ifs);
      }
      else
        throw invalid_argument("could not read file");
    };



    cout << "Enter port:" << endl;
    int port;
    cin >> port;
    server.config.port = port;

    cout << "Enter name:" << endl;
    string name;
    cin >> name;

    thread server_thread([&server]() {
        // Start server
        server.start();
    });


    nlohmann::json json_node_info;
    json_node_info["ip"] = "localhost";
    json_node_info["name"] = name;
    json_node_info["port"] = port; 

    profile_json_string = json_node_info.dump(); 


    auto r2 = seed.request("POST", "/registerNewNode", profile_json_string);
    cout << r2->content.rdbuf() << endl;
    bool exit = false;
    while(!exit){
        cout << "1.Show files" << endl;
        cout << "2.Download file" << endl;
        cout << "0.Exit" << endl;
        int num;
        cin >> num;
        if(num == 0){
            exit = true;
            cout << "Close: ";
        }
        else if(num == 1){
            vector<Node> nodes = get_nodes();
            map<Node,vector<string>> files = get_files(nodes,port);
            int count = 1;
            for (const auto& entry : files) {
                if(entry.second.size() > 0) {
                  for (const auto& name : entry.second){
                    cout << name << std::endl;
                  }
                  cout << endl;
                }
                else{
                  cout << "No files" << endl;
                }
            }
        }
        else if(num == 2){
          vector<Node> nodes = get_nodes();
            map<Node,vector<string>> files = get_files(nodes,port);
            int count = 1;
            for (const auto& entry : files) {
                for (const auto& name : entry.second){
                    cout << name << std::endl;
                }
                cout << endl;
            }
            cout << "Enter number of files to download: ";
            int input;
            cin >> input;
            for (const auto& entry : files) {
                for (const auto& name : entry.second){
                  if(name.rfind(to_string(input)) == 0){
                    size_t lastSpace = name.find_last_of(' ');
                    if (lastSpace != std::string::npos) {
                      std::string fileName = name.substr(lastSpace + 1);
                      string connectionString = entry.first.ip + ":" + to_string(entry.first.port);
                      HttpClient peer(connectionString);
                      auto r3 = peer.request("GET", "/download",fileName);     
                      string filePath = fs::current_path().string() + "/" + fileName;  
                      std::ofstream outputFile(filePath);       
                      if (outputFile.is_open()) {
                        outputFile << r3->content.rdbuf();
                        outputFile.close();
                        std::cout << "File written successfully." << std::endl;
                      } else {
                        std::cerr << "Error: Unable to create or write to the file." << std::endl;
                      }      
                    }
                  }
                }
                cout << endl;
            }
        }
        if(exit){
          auto offline = seed.request("POST", "/doOfflineNode", profile_json_string);
          cout << offline->content.rdbuf() << endl;
          server_thread.detach();
          break;
        }
    }
    server_thread.join();
}



    
//             string filePath = fs::current_path().string() + "/" + "index.html";

//             std::ofstream outputFile(filePath);

//             if (outputFile.is_open()) {
//                 outputFile << r3->content.rdbuf();
//                 outputFile.close();
//                 std::cout << "File created and content written successfully." << std::endl;
//             } else {
//                 std::cerr << "Error: Unable to create or write to the file." << std::endl;
//             }