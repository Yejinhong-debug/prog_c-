#include<iostream>
#include<string>
#include<vector>
#include<curl/curl.h> //for HTTP API
#include<nlohmann/json.hpp>
#include<thread>
#include<mutex>

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
	lock_guard<mutex>
		lock(curl_mutex);// Automatic lock/unlock

	CURL* curl;
	CURLcode res; //CURLcode: an enumeration type in libcurl
	string buffer;

	curl = curl_easy_init();// start the string
	if(curl)
	{
		string url = "https://jsonplaceholder.typicode.com/comments/" + to_string(comment_id);// get all comments
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
	output_tree["body"] = get_comment_body(input_tree["id"].get<int>());
	output_tree["replies"] = json::array();//only accept type string
	const json& replies = input_tree.value("replies", json::array());
	
	vector<thread> work;
	vector<json> results(replies.size());

	for(int i = 0; i<replies.size(); i++)
	{
		work.emplace_back([&replies, &results, i]()
			{
				results[i] = process_tree(replies[i]);
			});
	}
	for(auto& t : work)
	{
		if(t.joinable())
			t.join();
	}

	for(int i = 0; i<replies.size(); i++)
	{
		output_tree["replies"].push_back(process_tree(results[i]));
	}

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
