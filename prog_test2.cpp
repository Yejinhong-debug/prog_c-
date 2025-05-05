#include<iostream>
#include<string>
#include<vector>
#include<curl/curl.h> //for HTTP API
#include<nlohmann/json.hpp>
#include<thread>
#include<mutex>
#include<future>

using namespace std;
using json = nlohmann::ordered_json;//using json to call type json in nlohmann

mutex curl_mutex;
mutex cout_mutex;

//Callback function(to process the data from CURL)
static size_t CallBack(void *comment, size_t size, size_t num, string* s)
{
	size_t NewLen = size*num;
	try{
		s->append((char*)comment, NewLen);
		return NewLen;
	}
	catch(...)
	{
		return 0;
	}
}

// get signal comment
string get_comment_body(int comment_id)
{
	CURL* curl;
	CURLcode res; //CURLcode: an enumeration type in libcurl
	string buffer;

	curl = curl_easy_init();// start the string
	if(curl)
	{
		string url = "https://jsonplaceholder.typicode.com/comments/" + to_string(comment_id);// get all comments
				
		lock_guard<mutex>
			lock(curl_mutex);// Automatic lock/unlock
		curl_easy_setopt(curl, CURLOPT_URL, url.c_str());// set option for CURL, choose requring address
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, CallBack);// set function CallBack to recieve the data
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
		res = curl_easy_perform(curl);// save the result in res, data->buffer
		curl_easy_cleanup(curl);// clear out the data

		if(res != CURLE_OK)
			return "Error in getting comment";
		try{
			json j = json::parse(buffer);
			return j["body"].get<string>();// get body
		}
		catch(...)
		{
			return "Error in parsing response";
		}
	}
	return "CURL init failed";
}

//process the comment tree
json process_tree(const json& input_tree)
{
	json output_tree;
	output_tree["id"] = input_tree["id"];
		
	//implement asynchronous execution of programs
	auto body_future = async(launch::async, [&]() {
        	return get_comment_body(input_tree["id"].get<int>());
    	});
    	
	//process all replies
    	vector<future<json>> reply_futures;
    	for(const auto& reply : input_tree["replies"])
       	{
        	reply_futures.push_back(async(launch::async, [&reply]() {
            		return process_tree(reply);
        	}));
    	}
    	
	//wait for current reply
    	output_tree["body"] = body_future.get();
    	output_tree["replies"] = json::array();
	
	//collect all result of replies
    	for(auto& f : reply_futures)
        	output_tree["replies"].push_back(f.get());

	return output_tree;
}

int main()
{
	//initialize libcurl
	curl_global_init(CURL_GLOBAL_DEFAULT);

	try{
		json input_tree;
		cin >> input_tree;

		json output_tree = process_tree(input_tree);

		lock_guard<mutex>
			cout_lock(cout_mutex);
		cout << output_tree.dump(2) << endl;// dump(): json -> string;
	}
	catch(const exception& e)
	{
		lock_guard<mutex>
			cout_lock(cout_mutex);
		cerr << "Error: " << e.what() << endl;
		return 1;
	}

	curl_global_cleanup();
	return 0;
}
