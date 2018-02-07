**Path Planning Project**

The goal of this project is to design a path planner that is able to create smooth, safe paths for the car to follow along a 3 lane highway with traffic.

[//]: # (Image References)

[image_state_machine]: ./img/state_machine.png "State Machine"

### Model description

#### Creating the smooth path
One of the requirements is that the maximum jerk and acceleration are not exceeded. To achive this I created a smooth path using [the cubic spline](http://kluge.in-chemnitz.de/opensource/spline/). The simulator provides the car coordinates and the path points from the previuos iteration that have not been consumed yet. I used these points (if available) as part of the initial curve data plus another 3 points ahead of the current car position. The are spaced by 30m or 75m if the car is changing the line. When they were both the same, maximum jerk sometimes was exceeded, when the car was changing the lane during making a turn at the same time. Then I added points to the current path points.

The points on the spline are spaced in a way that they take into consideration current acceleration. So for example if the car is increasing the speed, the distance between the points will increase as well.

The whole caluclation was done in the car frame of reference with it in the center. And after the spline was created the points were translated back to the map frame of reference.

#### Acceleration and deceleration
If the car detects there is a vehicle in front of if (within 30m range) and it is not safe to change the lane it slows down to match its speed.

The acceleration is done by setting the new `goal_speed` so if the speed at the on of path is different than it the care slows down or speeds up.
This is achived by placing the points on the path with lower or higher distance.

#### Lange changes
The path planner uses a simple state machine with three states.

![image_state_machine]

First the planner finds out what are the safe actions it can be set of `change_lane_left` and `change_lane_right`. When the car has to slow down more than 5mph then the max speed and there is a safe action it deciedes to perform it. It prefers the `change_lane_left` as it is required by law in many countries that you can overtake only on the left on highways. For this project though this logic could be improved for example by choosing the one that has more free distance in front. The current logic seemed to be good enough. The planner goes to the default state (`stay_within_lane`) when the transition is complete, i.e. the car is 20 cm from the center of the lane.

### Sample run
I have recored a video with a little bit more than one lap. It can be downloaded from [https://downloads.clockworkbits.com/path_planning/video.mp4](https://downloads.clockworkbits.com/path_planning/video.mp4).

### Other - code and materials attribution

As the base implementation for the project I used code from the Udacity walktrough video (https://www.youtube.com/watch?v=7sI3VHFPP0w).