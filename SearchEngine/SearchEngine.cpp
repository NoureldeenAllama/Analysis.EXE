#include <iostream>
#include <algorithm>
#include <cctype>
#include <string>
#include <vector>
#include <fstream>
#include <map>

using namespace std;



/////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////       HELPER FUNCTIONS        //////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////

// reading CSV files into a string array
vector<string> read_csv(string filepath)
{
	vector<string> lines;

	fstream fin;
	fin.open(filepath, ios::in);

	if (fin.is_open())
	{
		string line;
		while (fin >> line)
		{
			lines.push_back(line);
		}

		fin.close();
	}

	else
	{
		cout << "Error in opening the File" << filepath;
	}


	return lines;
}

// Converting String to lower case and removing preceeding and trailing spaces
string process_word(string word)
{
	transform(word.begin(), word.end(), word.begin(), [](unsigned char c)
		{ return std::tolower(c); });

	word.erase(0, word.find_first_not_of(" "));
	word.erase(word.find_last_not_of(" ") + 1);

	return word;
}

// Split String into words using delimeter
vector<string> split(string word, string delimeter)
{
	vector<string> keywords;

	int start = 0;
	int end = word.find(delimeter);

	while (end != -1) {
		keywords.push_back(word.substr(start, end - start));
		start = end + delimeter.size();
		end = word.find(delimeter, start);
	}
	keywords.push_back(word.substr(start, end - start));


	return keywords;
}


// to find sites contains the query
bool contains_query(vector<string> keywords, string query)
{
	if (query[0] == '"' && query[query.length() - 1] == '"')
	{
		query = query.substr(1, query.length() - 2);

		if (count(keywords.begin(), keywords.end(), query))
		{
			return true;
		}
		else
		{
			return false;
		}
	}

	else if (query.find(" AND ") != string::npos)
	{
		vector<string> queries = split(query, " AND ");
		for (string i : queries)
		{
			i = process_word(i);
			if (!count(keywords.begin(), keywords.end(), i))
			{
				return false;
			}
		}
		return true;
	}

	else if (query.find(" OR ") != string::npos)
	{
		vector<string> queries = split(query, " OR ");
		for (string i : queries)
		{
			i = process_word(i);
			if (count(keywords.begin(), keywords.end(), i))
			{
				return true;
			}
		}
		return false;
	}

	else
	{
		vector<string> queries = split(query, " ");
		for (string i : queries)
		{
			i = process_word(i);
			if (count(keywords.begin(), keywords.end(), i))
			{
				return true;
			}
		}
		return false;
	}

}

// Search sites
vector<string> search(string query, vector<string> keywords_file)
{
	vector<string> sites;

	for (string i : keywords_file)
	{
		vector<string> values = split(i, ",");
		string site_name = values[0];
		values.erase(values.begin());

		if (contains_query(values, query))
		{
			sites.push_back(site_name);
		}
	}

	return sites;
}

//calc initial PR
map<string, double> calculate_initial_pr(vector<string> web_graph_file)
{
	map<string, int> outlinks;
	map<string, double> pr_score;
	map<string, double> updates;
	vector<string> sources;
	vector<string> destinations;
	vector<string> all_sites;

	for (string line : web_graph_file)
	{
		vector<string> values = split(line, ",");
		string src = values[0];
		string dst = values[1];

		sources.push_back(src);
		destinations.push_back(dst);

		if (find(all_sites.begin(), all_sites.end(), src) != all_sites.end())
			continue;
		else
			all_sites.push_back(src);

		if (find(all_sites.begin(), all_sites.end(), dst) != all_sites.end())
			continue;
		else
			all_sites.push_back(dst);
	}

	// building the outlinks map
	for (int i = 0; i < sources.size(); i++)
	{
		if (outlinks.count(sources[i]))
		{
			int n = outlinks[sources[i]] + 1;
			outlinks[sources[i]] = n;
		}
		else
		{
			outlinks[sources[i]] = 1;
		}
	}

	// adding the initial (PR score --> 1/total_sites) and (updates values)
	int total_sites = all_sites.size();
	double initial_pr = 1.0 / total_sites;

	for (int i = 0; i < all_sites.size(); i++)
	{
		pr_score.insert({ all_sites[i], initial_pr });

		int num_outlinks = outlinks[all_sites[i]];
		updates.insert({ all_sites[i], initial_pr / num_outlinks });
	}

	// updating pr_scores according to updates
	for (int i = 0; i < sources.size(); i++)
	{
		string src = sources[i];
		string dst = destinations[i];
		pr_score[dst] = pr_score[dst] + updates[src];
	}

	// noramlizing pr_score
	double total_pr = 0;
	for (int i = 0; i < all_sites.size(); i++)
	{
		total_pr = total_pr + pr_score[all_sites[i]];
	}

	for (int i = 0; i < all_sites.size(); i++)
	{
		pr_score[all_sites[i]] = pr_score[all_sites[i]] / total_pr;
	}


	return pr_score;
}

// calculating total pr_rank
map<string, double> total_rank(map<string, double> pr_score, vector<string> impressions_file, vector<string> clicks_file)
{
	map<string, double> total_rank;
	map<string, int> total_clicks;

	for (string line : clicks_file)
	{
		vector<string> values = split(line, ",");
		string site = values[0];
		int clicks = stoi(values[1]);
		total_clicks[site] = clicks;
	}

	// reading impressions
	for (string imp : impressions_file)
	{
		vector<string> values = split(imp, ",");
		string site = values[0];
		int impression = stoi(values[1]);

		// calculate total rank
		
		float imp_eqn = (0.1 * impression) / (1 + 0.1 * impression);
		float CTR = (float) total_clicks[site] / impression;
		float PRNorm = pr_score[site];



		float score = (0.4 * PRNorm) + ((1 - imp_eqn) * PRNorm + imp_eqn * CTR) * 0.6;


		// updating total_rank
		total_rank[site] = score;
	}


	return total_rank;
}

// sorting results
vector<string> sort(vector<string> results, vector<string> web_graph_file, vector<string> impressions_file, vector<string> clicks_file)
{
	map<string, double> pr = calculate_initial_pr(web_graph_file);
	map<string, double> total_ranks = total_rank(pr, impressions_file, clicks_file);

	for (int i = 0; i < results.size(); i++)
	{
		for (int j = i + 1; j < results.size(); j++)
		{
			if (total_ranks[results[j]] < total_ranks[results[i]])
			{
				string temp = results[i];
				results[i] = results[j];
				results[j] = temp;
			}
		}
	}
	
	return results;
}

// updating impressions
void update_impressions(vector<string> results, vector<string> &impressions_file, string filepath)
{
	fstream fout;
	fout.open(filepath, ios::out);


	for (string line : impressions_file)
	{
		vector<string> values = split(line, ",");
		string site = values[0];
		int imp = stoi(values[1]);

		if (find(results.begin(), results.end(), site) != results.end())
		{
			imp = imp + 1;
		}

		fout << site << "," << imp << endl;
	}
	fout.close();

	impressions_file = read_csv(filepath);
}

// updating clicks
void update_clicks(string result, vector<string>& clicks_file, string filepath)
{
	fstream fout;
	fout.open(filepath, ios::out);


	for (string line : clicks_file)
	{
		vector<string> values = split(line, ",");
		string site = values[0];
		int clicks = stoi(values[1]);

		if (result == site)
		{
			clicks++;
		}

		fout << site << "," << clicks << endl;
	}
	fout.close();

	clicks_file = read_csv(filepath);
}



// MAIN PROGRAM
int main()
{

	// READING CSV FILES
	vector<string> impressions_file = read_csv("related_files\\impressions.csv");
	vector<string> keywords_file = read_csv("related_files\\keyword.csv");
	vector<string> web_graph_file = read_csv("related_files\\web_graph.csv");

	vector<string> clicks_file = read_csv("related_files\\clicks.csv");

	vector<string> sorted_results;

	// VARIABLES
	char ans;
	string query;
	vector<string> results;

start:

	cout << "Welcome!" << endl;
	cout << "What would you like to do?" << endl;
	cout << "1. \t New Search" << endl;
	cout << "2. \t Exit" << endl;
	cout << endl;
	cout << "Type in your choice:  ";

	cin >> ans;
	cout << endl;

	if (ans == '2')
	{
	exit:

		// End the program !!
		cout << "Thank you for using the program...." << endl;
		return -1;
	}

	else if (ans == '1')
	{
	search:

		cout << "Type in the Search query:  ";
		getline(cin >> ws, query); // to accept spaces

		results = search(query, keywords_file);

		if (results.empty())
		{
			cout << "Sorry!! can't find any results.." << endl;
		}

		else
		{

			sorted_results = sort(results, web_graph_file, impressions_file, clicks_file);
			update_impressions(sorted_results,impressions_file, "related_files\\impressions.csv");

		show:

			cout << "Search results:" << endl;
			for (int i = 0; i < sorted_results.size(); i++)
			{
				cout << i + 1 << ".\t" << sorted_results[i] << endl;
			}
			cout << endl;
		}

		cout << "Would you like to" << endl;
		cout << "1. \t Choose a webpage to open" << endl;
		cout << "2. \t New Search" << endl;
		cout << "3. \t Exit" << endl;
		cout << endl;
		cout << "Type in your choice: ";

		cin >> ans;
		cout << endl;

		if (ans == '1')
		{
			int page_no;
			cout << "Please choose a webpage to open: ";
			cin >> page_no;
			string site = sorted_results[page_no - 1];
			update_clicks(site, clicks_file, "related_files//clicks.csv");

			cout << endl;
			cout << "You're viewing " << site << endl;
			cout << "Would you like to" << endl;
			cout << "1. \t Back to search results" << endl;
			cout << "2. \t New search" << endl;
			cout << "3. \t Exit" << endl;
			cout << endl;
			cout << "Type in your choice: ";
			cin >> ans;

			if (ans == '1')
			{
				goto show;
			}
			else if (ans == '2')
			{
				goto search;
			}
			else if (ans == '3')
			{
				goto exit;
			}
			else
			{
				cout << "Invalid Input..." << endl;
				goto start;
			}

		}
		if (ans == '2')
		{
			goto search;
		}
		else if (ans == '3')
		{
			goto exit;
		}

	}

	else
	{
		cout << "Sorry, you typed wrong answer please make sure you to choose 1 or 2.." << endl;
		goto start;
	}


	return 0;


}

