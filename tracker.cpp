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

struct Node {
    string ip;
    int port;
    string name;
    bool status;

    friend bool operator==(const Node &lhs,const Node &rhs){
        if(lhs.ip == rhs.ip && lhs.port == rhs.port && lhs.name == rhs.name){
            return true;
        }
        return false;
    }
    friend bool operator!=(const Node &lhs, const Node &rhs) {
        return !(lhs == rhs);
    }
};

vector<Node> nodes;


bool contains(Node node) {
    for(Node& value : nodes){
            if(value == node){
                value.status = true;
                return true;
        }
    }
    return false;
}

Node* find(Node node) {
    for(Node& value : nodes){
            if(value == node){
                return &value;
        }
    }
    return NULL;
}

void deleteNode(const Node node){
    nodes.erase(
        std::remove_if(nodes.begin(), nodes.end(), [&](Node const & next) {
            return node == next;
        }),
        nodes.end());
}

string vector_to_json(vector<Node> nodes_vec){
    json jsonArray = json::array();
    for (const Node& value : nodes_vec) {
        json nodeJson;
            nodeJson["ip"] = value.ip;
            nodeJson["port"] = value.port;
            nodeJson["name"] = value.name;
            jsonArray.push_back(nodeJson);
    }
    std::string jsonString = jsonArray.dump();
    return jsonString;
}

vector<Node> getOnlineNodes(Node &defiantNode){
    vector<Node> result;
    for (const Node& value : nodes) {
        if(value.status && value != defiantNode){
            result.push_back(value);
        }
    }
    return result;
}

int main(){
    HttpServer server;
    server.config.port = 8080;
    namespace fs = std::filesystem;


    server.resource["^/registerNewNode"]["POST"] = [](shared_ptr<HttpServer::Response> response, shared_ptr<HttpServer::Request> request) {
        json content = json::parse(request->content);
        string name = content["name"].get<string>();
        int port = content["port"].get<int>();
        string ip = content["ip"].get<string>();
        struct Node node = {ip,port,name};
        if(contains(node)){
            cout << "Welcome back node..." << endl;
            cout << "IP: " << ip << " Port: " << port << " Name: " << name << endl;
            *response << "HTTP/1.1 200 OK\r\nContent-Length: " << request->content.string().length() << "\r\n\r\n"
                << "Welcom again....";
        }
        else{
            node.status = true;
            nodes.push_back(node);
            cout << "New node added..." << endl;
            cout << "IP: " << ip << " Port: " << port << " Name: " << name << endl;
            *response << "HTTP/1.1 200 OK\r\nContent-Length: " << request->content.string().length() << "\r\n\r\n"
                << "Success registration";
        }
        
    };

    server.resource["^/doOfflineNode"]["POST"] = [](shared_ptr<HttpServer::Response> response, shared_ptr<HttpServer::Request> request) {
        json content = json::parse(request->content);
        string name = content["name"].get<string>();
        int port = content["port"].get<int>();
        string ip = content["ip"].get<string>();
        struct Node node = {ip,port,name};
        Node* find_node;
        if((find_node = find(node)) != NULL){
            find_node->status = false;
        }
        for(Node& value : nodes){
           cout << "IP: " << value.ip << " Port: " << value.port << " Name: " << value.name << endl;
        }
        *response << "HTTP/1.1 200 OK\r\nContent-Length: " << request->content.string().length() << "\r\n\r\n"
                << "Success exit...";
        cout << "Node exit: " << endl;
        cout << "IP: " << ip << " Port: " << port << " Name: " << name << endl;
    };

    server.resource["^/onlineNodes"]["GET"] = [](shared_ptr<HttpServer::Response> response, shared_ptr<HttpServer::Request> request) {
        json content = json::parse(request->content);
        string name = content["name"].get<string>();
        int port = content["port"].get<int>();
        string ip = content["ip"].get<string>();
        struct Node node = {ip,port,name};
        string jsonString = vector_to_json(getOnlineNodes(node));
        response->write(jsonString);
    };

    thread server_thread([&server]() {
        cout << "Server started....." << endl;
        server.start();
    });

    server_thread.join();
}