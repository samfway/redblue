#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"

#include <sys/stat.h>
#include <unistd.h>
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>

using namespace rapidjson;

/* 
 *  Usage:
 *  halo <data_directory> <start_index> <stop_index> <output_file>
 *
 *  <data_directory> - dir containing .details files
 *  <start_index>    - (int) of first .details file
 *  <stop_index>     - (int) of last .details file
 *  <output_file>    - output file for extracted info
 *
 *  Output:
 *  A text file (<output_file>), each line represents a single match.
 *  
 *  Output format:
 *  "game_id,num_players,red_pts,blue_pts,red_bails,blue_bails,largest_diff,basemap\n"
 *  game_id      - game identifier
 *  num_players  - total number of players in the game
 *  red_pts      - Number of points scored by the RED team (team 0)
 *  blue_pts     - Number of points scored by the BLUE team (team 1)
 *  red_bails    - number of early exits by RED team
 *  blue_bails   - number of early exits by BLUE team
 *  largest_diff - largest time between kills on either team 
 *  basemap      - Name of the BaseMap
 */

/*
 * Known issues with game files:
 *   Game details can be missing altogether, JSON files corrupt.
 *   Games can be customized with non-standard rules, maps, weapons.
 *   Scores can inexplicably jump up, rather than incrementing by one
 *   Lists of teams, players, and kills can be corrupt, missing information.
 *   Games can have corrupt timestamps, massive amounts of inactivity.
 */

// Test if the file exists.
inline bool exists(const std::string& name) {
    struct stat buffer;   
    return (stat (name.c_str(), &buffer) == 0); 
}

int main(int argc, char** argv) {

    int i, j, k;
    int beg = atoi(argv[2]);
    int end = atoi(argv[3]);
    std::ofstream outputFile(argv[4]);
    int score0;
    int score1;
    int total_score;
    int bails0;
    int bails1;
    int num_teams, team, num_kills;
    Value *kills;
    bool red_kills_logged, blue_kills_logged, all_players_logged;
    int num_players, num_player_records;
    long int largest_diff, temp_diff;
    ParseResult ok;
    int red_team_count, blue_team_count, red_scores, blue_scores;

    for (k = beg; k <= end; k++)
    {
        std::ostringstream file_name;  
        file_name << argv[1] << k << ".details"; 
    
        try
        {
            if (exists(file_name.str()))
            {
                // Read and parse the JSON file
                std::ifstream in(file_name.str());
                std::string buffer((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
                Document d;
                ok = d.Parse(buffer.c_str());

                if (ok &&
                    strcmp(d["reason"].GetString(), "Okay") == 0 &&                                 // Game ended "Okay"
                    // strcmp(d["GameDetails"]["PlaylistName"].GetString(), "Custom Game") != 0 &&     // Playlist is *not* "Custom Game"
                    strcmp(d["GameDetails"]["PlaylistName"].GetString(), "Team Slayer") == 0 &&     // Playlist is *exactly* Team Slayer
                    d["status"] == 0 &&                                                             // Game ended normally
                    strcmp(d["GameDetails"]["GameVariantName"].GetString(), "Slayer") == 0 &&       // Game Variant Name is Slayer
                    d["GameDetails"]["IsTeamGame"] == true)                                         // Detailed info is logged
                {

                    // std::cout << k << std::endl;

                    const Value& teams = d["GameDetails"]["Teams"];
                    if (teams.IsArray())
                    {
                        num_teams = teams.Size();
                        if (num_teams == 2)
                        {
                            bails0 = 0;
                            bails1 = 0;
                            score0 = d["GameDetails"]["Teams"][0]["Score"].GetInt();
                            score1 = d["GameDetails"]["Teams"][1]["Score"].GetInt();
                            num_players = d["GameDetails"]["PlayerCount"].GetInt();
                            total_score = score0 + score1;
                            const Value& players = d["GameDetails"]["Players"];
                            num_player_records = players.Size();
                            all_players_logged = num_player_records == num_players;

                            // There should be at least as many kills recorded as there are points
                            kills = &d["GameDetails"]["Teams"][0]["KillsOverTime"];
                            num_kills = kills->Size();
                            red_kills_logged = num_kills >= d["GameDetails"]["Teams"][0]["TeamTotalKills"].GetInt();

                            kills = &d["GameDetails"]["Teams"][1]["KillsOverTime"];
                            num_kills = kills->Size();
                            blue_kills_logged = num_kills >= d["GameDetails"]["Teams"][1]["TeamTotalKills"].GetInt();

                            // Longest period between kills
                            largest_diff = -1;

                            for (j=0; j<2; j++)
                            {
                                kills = &d["GameDetails"]["Teams"][j]["KillsOverTime"];
                                for (i=1; i < kills->Size(); i++)
                                {
                                    temp_diff = d["GameDetails"]["Teams"][j]["KillsOverTime"][i][0].GetInt64() - 
                                                d["GameDetails"]["Teams"][j]["KillsOverTime"][i-1][0].GetInt64();
                                    if (temp_diff > largest_diff)
                                    {
                                        largest_diff = temp_diff;
                                    }
                                }
                            }

                            // std::cout << "HERE (largest diff)" << std::endl;

                            red_team_count = 0;
                            blue_team_count = 0; 
                            red_scores = 0;
                            blue_scores = 0;
                            for (j=0; j<players.Size(); j++)
                            {
                                if (d["GameDetails"]["Players"][j]["Team"].GetInt() == 0)
                                {
                                    red_team_count += 1;
                                    red_scores += d["GameDetails"]["Players"][j]["Score"].GetInt();
                                }
                                else
                                {
                                    blue_team_count += 1;
                                    blue_scores += d["GameDetails"]["Players"][j]["Score"].GetInt();
                                }
                            }

                            // std::cout << "HERE (kills logged)" << std::endl;

                            if (total_score > 0 && red_kills_logged && blue_kills_logged && largest_diff > 0 &&
                                red_team_count == blue_team_count && red_scores == score0 && blue_scores == score1 &&
                                all_players_logged)
                            {
                                for (SizeType i = 0; i < players.Size(); i++)
                                {
                                    if (d["GameDetails"]["Players"][i]["DNF"] == true)
                                    {
                                        team = d["GameDetails"]["Players"][i]["Team"].GetInt();
                                        if (team == 0)
                                        {
                                            bails0++;
                                        }
                                        else
                                        {
                                            bails1++;
                                        }
                                    }
                                }
                        
                                //  "game_id,num_players,red_pts,blue_pts,red_bails,blue_bails,largest_diff,basemap\n"
                                outputFile << k << "," 
                                           << num_players << "," 
                                           << score0 << "," 
                                           << score1 << ","
                                           << bails0 << "," 
                                           << bails1 << ","
                                           << largest_diff << ","
                                           << d["GameDetails"]["BaseMapName"].GetString() << std::endl;
                            }
                        }
                    }
                }
            }
        }
        catch (...)
        {
            std::cout << "Error processing " << file_name << std::endl; 
        }
    }
    outputFile.close();
}
