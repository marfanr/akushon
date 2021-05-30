// Copyright (c) 2021 Ichiro ITS
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
// THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#include <rclcpp/rclcpp.hpp>

#include <akushon/action_manager.hpp>
#include <akushon/pose.hpp>

#include <iostream>
#include <string>
#include <memory>

int main(int argc, char * argv[])
{
  // init rclcpp
  // rclcpp::init(argc, argv);

  // init node
  auto action_manager =
    std::make_shared<akushon::ActionManager>("action_manager", "motion_manager");

  std::vector<std::string> action_names = {
      "left_kick", 
      "right_kick", 
      "back_standup", 
      "forward_standup", 
      "left_standup",
      "right_standup"
    }; 

  action_manager->load_action_data(action_names);
  int action_id = 0;

  std::string line;
  std::getline(std::cin, line);

  do 
  {
    std::istringstream iss(line);
    std::vector<std::string> cmds;
    std::string cmd;

    while(iss >> cmd) {
      cmds.push_back(cmd);
    }

    if (cmds.size() == 2) {
      if (cmds[0] == "run_action") {
        bool find = false;
        for (int id = 0; id < action_names.size(); id++) {
          if (cmds[1] == action_names[id]) {
            find = true;
            action_id = id;

            // !! need to load action list before get the desired action
            auto action = action_manager->get_action(action_id);

            bool done_step_cmd = false;
            std::string step_cmd;
            std::cout << "step : ";
            std::cin >> step_cmd;

            if (step_cmd == "all") {
              // move joints
              while (action->is_finished()) {
                akushon::Pose pose = action->get_pose();

                // check the node
                if (action_manager->is_ready()) {
                  // send request
                  auto response_future =
                    action_manager->send_joints_request(pose.get_joints(), pose.get_speed());

                  // wait for response
                  if (rclcpp::spin_until_future_complete(action_manager, response_future) ==
                    rclcpp::executor::FutureReturnCode::SUCCESS)
                  {
                    auto response = response_future.get();
                    if (static_cast<bool>(response->status)) {
                      action->next_pose();
                    } else {
                      std::cout << "failed to move joints at pose: " << pose.get_name() << "\n";
                      break;
                    }
                  }
                } else {
                  std::cout << "waiting...\n";
                }
              }
            } 
            else if (std::stoi(step_cmd) >= 0 &&  std::stoi(step_cmd) <= 6){
              akushon::Pose pose = action->get_pose();

              // check the node
              if (action_manager->is_ready()) {
                // send request
                auto response_future =
                  action_manager->send_joints_request(pose.get_joints(), pose.get_speed());

                // wait for response
                if (rclcpp::spin_until_future_complete(action_manager, response_future) ==
                  rclcpp::executor::FutureReturnCode::SUCCESS)
                {
                  auto response = response_future.get();
                  if (static_cast<bool>(response->status)) {
                    std::cout << "succeed to move joints at pose: " << pose.get_name() << "\n";
                  } else {
                    std::cout << "failed to move joints at pose: " << pose.get_name() << "\n";
                  }
                }
              } else {
                std::cout << "waiting...\n";
              }
            } 
            else {
              std::cout << "-ERR step is not defined";
            }
            break; // done pose
          }
        }
        // if doesn't find the action_name 
        if (!find) {
          std::cout << "-ERR second command is not valid\n usage: action_run <action_name>";
        }
      } else {
        std::cout << "-ERR first command is not valid\n usage: action_run <action_name>";
      }
    }
    else {
      std::cout << "-ERR too much command\n usage: action_run <action_name>";
    }

    std::getline(std::cin, line); // get new action
    action_manager->load_action_data(action_names); // reload data
  }
  while(line != "q");

  rclcpp::shutdown();
}
