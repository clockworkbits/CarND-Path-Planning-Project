#include <fstream>
#include <math.h>
#include <uWS/uWS.h>
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>
#include <set>
#include "Eigen-3.3/Eigen/Core"
#include "Eigen-3.3/Eigen/QR"
#include "json.hpp"
#include "spline.h"
#include "waypoint.h"
#include "vehicle.h"
#include "action.h"

using namespace std;

// for convenience
using json = nlohmann::json;

// For converting back and forth between radians and degrees.
constexpr double pi() { return M_PI; }
double deg2rad(double x) { return x * pi() / 180; }
double rad2deg(double x) { return x * 180 / pi(); }

// Checks if the SocketIO event has JSON data.
// If there is data the JSON object in string format will be returned,
// else the empty string "" will be returned.
string hasData(string s) {
  auto found_null = s.find("null");
  auto b1 = s.find_first_of("[");
  auto b2 = s.find_first_of("}");
  if (found_null != string::npos) {
    return "";
  } else if (b1 != string::npos && b2 != string::npos) {
    return s.substr(b1, b2 - b1 + 2);
  }
  return "";
}

double distance(double x1, double y1, double x2, double y2)
{
	return sqrt((x2-x1)*(x2-x1)+(y2-y1)*(y2-y1));
}
int ClosestWaypoint(double x, double y, const vector<double> &maps_x, const vector<double> &maps_y)
{

	double closestLen = 100000; //large number
	int closestWaypoint = 0;

	for(int i = 0; i < maps_x.size(); i++)
	{
		double map_x = maps_x[i];
		double map_y = maps_y[i];
		double dist = distance(x,y,map_x,map_y);
		if(dist < closestLen)
		{
			closestLen = dist;
			closestWaypoint = i;
		}

	}

	return closestWaypoint;

}

int NextWaypoint(double x, double y, double theta, const vector<double> &maps_x, const vector<double> &maps_y)
{

	int closestWaypoint = ClosestWaypoint(x,y,maps_x,maps_y);

	double map_x = maps_x[closestWaypoint];
	double map_y = maps_y[closestWaypoint];

	double heading = atan2((map_y-y),(map_x-x));

	double angle = fabs(theta-heading);
  angle = min(2*pi() - angle, angle);

  if(angle > pi()/4)
  {
    closestWaypoint++;
  if (closestWaypoint == maps_x.size())
  {
    closestWaypoint = 0;
  }
  }

  return closestWaypoint;
}

// Transform from Cartesian x,y coordinates to Frenet s,d coordinates
vector<double> getFrenet(double x, double y, double theta, const vector<double> &maps_x, const vector<double> &maps_y)
{
	int next_wp = NextWaypoint(x,y, theta, maps_x,maps_y);

	int prev_wp;
	prev_wp = next_wp-1;
	if(next_wp == 0)
	{
		prev_wp  = maps_x.size()-1;
	}

	double n_x = maps_x[next_wp]-maps_x[prev_wp];
	double n_y = maps_y[next_wp]-maps_y[prev_wp];
	double x_x = x - maps_x[prev_wp];
	double x_y = y - maps_y[prev_wp];

	// find the projection of x onto n
	double proj_norm = (x_x*n_x+x_y*n_y)/(n_x*n_x+n_y*n_y);
	double proj_x = proj_norm*n_x;
	double proj_y = proj_norm*n_y;

	double frenet_d = distance(x_x,x_y,proj_x,proj_y);

	//see if d value is positive or negative by comparing it to a center point

	double center_x = 1000-maps_x[prev_wp];
	double center_y = 2000-maps_y[prev_wp];
	double centerToPos = distance(center_x,center_y,x_x,x_y);
	double centerToRef = distance(center_x,center_y,proj_x,proj_y);

	if(centerToPos <= centerToRef)
	{
		frenet_d *= -1;
	}

	// calculate s value
	double frenet_s = 0;
	for(int i = 0; i < prev_wp; i++)
	{
		frenet_s += distance(maps_x[i],maps_y[i],maps_x[i+1],maps_y[i+1]);
	}

	frenet_s += distance(0,0,proj_x,proj_y);

	return {frenet_s,frenet_d};

}

// Transform from Frenet s,d coordinates to Cartesian x,y
vector<double> getXY(double s, double d, const vector<Waypoint> &maps_waypoints)
{
	int prev_wp = -1;

	while(s > maps_waypoints[prev_wp+1].s && (prev_wp < (int)(maps_waypoints.size()-1) ))
	{
		prev_wp++;
	}

	int wp2 = (prev_wp+1)%maps_waypoints.size();

	double heading = atan2((maps_waypoints[wp2].y-maps_waypoints[prev_wp].y),(maps_waypoints[wp2].x-maps_waypoints[prev_wp].x));
	// the x,y,s along the segment
	double seg_s = (s-maps_waypoints[prev_wp].s);

	double seg_x = maps_waypoints[prev_wp].x+seg_s*cos(heading);
	double seg_y = maps_waypoints[prev_wp].y+seg_s*sin(heading);

	double perp_heading = heading-pi()/2;

	double x = seg_x + d*cos(perp_heading);
	double y = seg_y + d*sin(perp_heading);

	return {x,y};

}

void print(const string &name, const vector<double> &data) {
	std::cout << name << " : ";

	for (auto i = data.begin(); i != data.end(); ++i)
		std::cout << *i << ' ';

	std::cout << std::endl;
}

int main() {
  uWS::Hub h;

  // Load up map values for waypoint's x,y,s and d normalized normal vectors
  vector<Waypoint> map_waypoints;

  // Waypoint map to read from
  string map_file_ = "../data/highway_map.csv";
  // The max s value before wrapping around the track back to 0
  double max_s = 6945.554;

  ifstream in_map_(map_file_.c_str(), ifstream::in);

  string line;
  while (getline(in_map_, line)) {
  	istringstream iss(line);
    Waypoint waypoint;
  	iss >> waypoint.x;
  	iss >> waypoint.y;
  	iss >> waypoint.s;
  	iss >> waypoint.d_x;
  	iss >> waypoint.d_y;
    map_waypoints.push_back(waypoint);
  }

	int lane = 1;

	const double delta_t = 0.02;

	const double max_speed = 49.5 / 2.24; // in m/s
	const double acceleration = 7.0; // in m/s^2

	double goal_speed = max_speed; // in m/s

	double car_speed_at_end_of_path = 0.0;

  Action action_at_end_of_path = stay_within_lane;

	h.onMessage([&map_waypoints, &lane, &delta_t, &max_speed, &acceleration, &car_speed_at_end_of_path, &goal_speed, &action_at_end_of_path](uWS::WebSocket<uWS::SERVER> ws, char *data, size_t length,
                     uWS::OpCode opCode) {
    // "42" at the start of the message means there's a websocket message event.
    // The 4 signifies a websocket message
    // The 2 signifies a websocket event
    //auto sdata = string(data).substr(0, length);
    //cout << sdata << endl;
    if (length && length > 2 && data[0] == '4' && data[1] == '2') {

      auto s = hasData(data);

      if (s != "") {
        auto j = json::parse(s);
        
        string event = j[0].get<string>();
        
        if (event == "telemetry") {
          // j[1] is the data JSON object
          
        	// Main car's localization Data
          	double car_x = j[1]["x"];
          	double car_y = j[1]["y"];
          	double car_s = j[1]["s"];
          	double car_d = j[1]["d"];
          	double car_yaw = j[1]["yaw"];
          	double car_speed = j[1]["speed"];

          	// Previous path data given to the Planner
          	auto previous_path_x = j[1]["previous_path_x"];
          	auto previous_path_y = j[1]["previous_path_y"];
          	// Previous path's end s and d values 
          	double end_path_s = j[1]["end_path_s"];
          	double end_path_d = j[1]["end_path_d"];

          	// Sensor Fusion Data, a list of all other cars on the same side of the road.
          	auto sensor_fusion = j[1]["sensor_fusion"];

          	json msgJson;

          //std::cout << "previous size: " << previous_path_x.size() << std::endl;

					//std::cout << "sensor fusion size: " << sensor_fusion.size() << std::endl;/**/
					// Convert the sensor fusion data into the vehicles

					vector<Vehicle> vehicles;

					for (int i = 0; i < sensor_fusion.size(); i++) {
						Vehicle v(sensor_fusion[i][0], // id
											sensor_fusion[i][1], // x
											sensor_fusion[i][2], // y
											sensor_fusion[i][3], // vx
											sensor_fusion[i][4], // vy
											sensor_fusion[i][5], // s
											sensor_fusion[i][6]); // d
						vehicles.push_back(v);
					}


//					std::cout << "   Vehicles list   " << std::endl;
//					for (Vehicle v : vehicles) {
//						std::cout << "Vehicle id = " << v.id << " speed = " << v.speed();
//						for (int l = 0; l < 3; l++) {
//							if (v.IsInLane(l)) {
//								std::cout << " in lane = " << l;
//							}
//						}
//						std::cout << std::endl;
//					}

					int previous_size = previous_path_x.size();

          if (action_at_end_of_path == change_lane_left || action_at_end_of_path == change_lane_right) {
            if (end_path_d > 1.8 + 4 * lane && end_path_d < 2.2 + 4 * lane) {
              action_at_end_of_path = stay_within_lane;
            }
          }

          // Is safe to change lane left?
          std::set<Action> safe_actions;
          if (action_at_end_of_path == stay_within_lane) {
            int left_lane = lane - 1;
            if (left_lane >= 0) {

              Vehicle *car_in_front = nullptr;
              Vehicle *car_behind = nullptr;

              for (Vehicle &v : vehicles) {
                if(v.IsInLane(left_lane)) {
                  double vehicle_future_s = v.FutureS(0.02 * previous_size);

                  // Find the car in front of us
                  if (vehicle_future_s > end_path_s) {
                    if (car_in_front == nullptr) {
                      car_in_front = &v;
                    } else if (vehicle_future_s < car_in_front->FutureS(0.02 * previous_size)) {
                      car_in_front = &v;
                    }
                  }

                  // Find the car behind us
                  if (vehicle_future_s < end_path_s) {
                    if (car_behind == nullptr) {
                      car_behind = &v;
                    } else if (vehicle_future_s > car_behind->FutureS(0.02 * previous_size)) {
                      car_behind = &v;
                    }
                  }
                }
              }

              if (car_behind != nullptr) {
                std::cout << "Found car behind (left) " << car_behind->id << std::endl;
              }

              bool safe_front_distance = car_in_front == nullptr || car_in_front->FutureS(0.02 * previous_size) - end_path_s > 50.0;
              bool safe_behind_distance = car_behind == nullptr || end_path_s - car_behind->FutureS(0.02 * previous_size) > 10.0;

              if (safe_front_distance && safe_behind_distance) {
                safe_actions.insert(change_lane_left);
              }

            }

            int right_lane = lane + 1;
            if (right_lane < 3) {
              Vehicle *car_in_front = nullptr;
              Vehicle *car_behind = nullptr;

              for (Vehicle &v : vehicles) {
                if(v.IsInLane(right_lane)) {
                  double vehicle_future_s = v.FutureS(0.02 * previous_size);

                  // Find the car in front of us
                  if (vehicle_future_s > end_path_s) {
                    if (car_in_front == nullptr) {
                      car_in_front = &v;
                    } else if (vehicle_future_s < car_in_front->FutureS(0.02 * previous_size)) {
                      car_in_front = &v;
                    }
                  }

                  // Find the car behind us
                  if (vehicle_future_s < end_path_s) {
                    if (car_behind == nullptr) {
                      car_behind = &v;
                    } else if (vehicle_future_s > car_behind->FutureS(0.02 * previous_size)) {
                      car_behind = &v;
                    }
                  }
                }
              }

              if (car_behind != nullptr) {
                std::cout << "Found car behind " << car_behind->id << std::endl;
              }

              bool safe_front_distance = car_in_front == nullptr || car_in_front->FutureS(0.02 * previous_size) - end_path_s > 50.0;
              bool safe_behind_distance = car_behind == nullptr || end_path_s - car_behind->FutureS(0.02 * previous_size) > 10.0;

              if (safe_front_distance && safe_behind_distance) {
                safe_actions.insert(change_lane_right);
                if (car_behind != nullptr) {
                  std::cout << "Car behind " << car_behind->id << " distance = " << end_path_s - car_behind->FutureS(0.02 * previous_size) << std::endl;
                }
              }
            }
          }

          double new_goal_speed = goal_speed;

					// Check if there is a car in front of us
					for (Vehicle v : vehicles) {
						double vehicle_future_s = v.FutureS(0.02 * previous_size);
						if (v.IsInLane(lane)
								&& vehicle_future_s > end_path_s
								&& vehicle_future_s - end_path_s < 30.0) {
              new_goal_speed = min(new_goal_speed, v.speed());
							//std::cout << "Slowing down to = " << new_goal_speed << " because of car id = " << v.id << std::endl;

              goal_speed = new_goal_speed;

              if (new_goal_speed < max_speed - 5 / 2.24 && action_at_end_of_path == stay_within_lane) {
                if (safe_actions.find(change_lane_left) != safe_actions.end()) {
                  lane = lane - 1;
                  action_at_end_of_path = change_lane_left;
                } else if (safe_actions.find(change_lane_right) != safe_actions.end()) {
                  lane = lane + 1;
                  action_at_end_of_path = change_lane_right;
                }
              }
						}
					}

					// No one in front of us - we can accelerate
					int vehicles_in_front_count = 0;

					for (Vehicle v : vehicles) {
						double vehicle_future_s = v.FutureS(0.02 * previous_size);


						if (v.IsInLane(lane)
								&& vehicle_future_s > end_path_s
								&& vehicle_future_s - end_path_s < 50.0) {
							vehicles_in_front_count++;
						}
					}

					if (vehicles_in_front_count == 0) {
						goal_speed = max_speed;
						//std::cout << "No on in front of us - let's accelerate"  << std::endl;
					}

					// Generate the path
					vector<double> points_x;
					vector<double> points_y;

					double ref_x = car_x;
					double ref_y = car_y;
					double ref_yaw = deg2rad(car_yaw);

					if (previous_size == 0) {
						car_speed_at_end_of_path = car_speed / 2.24;
					}

					if (previous_size < 2) {
						double prev_car_x = car_x - cos(deg2rad(car_yaw));
						double prev_car_y = car_y - sin(deg2rad(car_yaw));

						points_x.push_back(prev_car_x);
						points_x.push_back(car_x);

						points_y.push_back(prev_car_y);
						points_y.push_back(car_y);
					} else {
						ref_x = previous_path_x[previous_size - 1];
						ref_y = previous_path_y[previous_size - 1];

						double ref_x_prev = previous_path_x[previous_size - 2];
						double ref_y_prev = previous_path_y[previous_size - 2];

						ref_yaw = atan2(ref_y - ref_y_prev, ref_x - ref_x_prev);

						points_x.push_back(ref_x_prev);
						points_x.push_back(ref_x);

						points_y.push_back(ref_y_prev);
						points_y.push_back(ref_y);
					}

					double dist = 30.0;

					if (action_at_end_of_path == change_lane_left || action_at_end_of_path == change_lane_right) {
						dist = 75.0;
					}

					vector<double> next_wp0 = getXY(car_s + dist, 2 + 4 * lane, map_waypoints);
					vector<double> next_wp1 = getXY(car_s + dist * 2, 2 + 4 * lane, map_waypoints);
					vector<double> next_wp2 = getXY(car_s + dist * 3, 2 + 4 * lane, map_waypoints);

					points_x.push_back(next_wp0[0]);
					points_x.push_back(next_wp1[0]);
					points_x.push_back(next_wp2[0]);

					points_y.push_back(next_wp0[1]);
					points_y.push_back(next_wp1[1]);
					points_y.push_back(next_wp2[1]);

					for (int i = 0; i < points_x.size(); i++) {
						double shift_x = points_x[i] - ref_x;
						double shift_y = points_y[i] - ref_y;

						points_x[i] = shift_x * cos(0 - ref_yaw) - shift_y * sin(0 - ref_yaw);
						points_y[i] = shift_x * sin(0 - ref_yaw) + shift_y * cos(0 - ref_yaw);
					}


					// spline
					tk::spline s;

					s.set_points(points_x, points_y);

					vector<double> next_x_vals;
					vector<double> next_y_vals;

					for (int i = 0; i < previous_path_x.size(); i++) {
						next_x_vals.push_back(previous_path_x[i]);
						next_y_vals.push_back(previous_path_y[i]);
					}

					double target_x = 30.0;
					double target_y = s(target_x);
					double target_dist = distance(0, 0, target_x, target_y);

					double x_add_on = 0.0;

					for (int i = 0; i <= 50 - previous_path_x.size(); i++) {
						if (car_speed_at_end_of_path < goal_speed) {
							car_speed_at_end_of_path += delta_t * acceleration;
						} else if (car_speed_at_end_of_path > goal_speed) {
							car_speed_at_end_of_path -= delta_t * acceleration;
						}

						double x_point = x_add_on + car_speed_at_end_of_path * delta_t;

						if (x_point > target_x) {
							break;
						}

						double y_point = s(x_point);

						x_add_on = x_point;

						double x_orig = x_point;
						double y_orig = y_point;

						x_point = x_orig * cos(ref_yaw) - y_orig * sin(ref_yaw);
						y_point = x_orig * sin(ref_yaw) + y_orig * cos(ref_yaw);

						x_point += ref_x;
						y_point += ref_y;

						next_x_vals.push_back(x_point);
						next_y_vals.push_back(y_point);
					}

					msgJson["next_x"] = next_x_vals;
					msgJson["next_y"] = next_y_vals;

					auto msg = "42[\"control\","+ msgJson.dump()+"]";

					//this_thread::sleep_for(chrono::milliseconds(1000));
					ws.send(msg.data(), msg.length(), uWS::OpCode::TEXT);
          
        }
      } else {
        // Manual driving
        std::string msg = "42[\"manual\",{}]";
        ws.send(msg.data(), msg.length(), uWS::OpCode::TEXT);
      }
    }
  });

  // We don't need this since we're not using HTTP but if it's removed the
  // program
  // doesn't compile :-(
  h.onHttpRequest([](uWS::HttpResponse *res, uWS::HttpRequest req, char *data,
                     size_t, size_t) {
    const std::string s = "<h1>Hello world!</h1>";
    if (req.getUrl().valueLength == 1) {
      res->end(s.data(), s.length());
    } else {
      // i guess this should be done more gracefully?
      res->end(nullptr, 0);
    }
  });

  h.onConnection([&h](uWS::WebSocket<uWS::SERVER> ws, uWS::HttpRequest req) {
    std::cout << "Connected!!!" << std::endl;
  });

  h.onDisconnection([&h](uWS::WebSocket<uWS::SERVER> ws, int code,
                         char *message, size_t length) {
    ws.close();
    std::cout << "Disconnected" << std::endl;
  });

  int port = 4567;
  if (h.listen(port)) {
    std::cout << "Listening to port " << port << std::endl;
  } else {
    std::cerr << "Failed to listen to port" << std::endl;
    return -1;
  }
  h.run();
}
