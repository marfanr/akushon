// Copyright (c) 2021 Ichiro ITS
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include <chrono>
#include <iostream>
#include <iomanip>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "akushon/action/node/action_node.hpp"

#include "akushon/action/node/action_manager.hpp"
#include "akushon/action/model/action_name.hpp"
#include "akushon/action/model/pose.hpp"
#include "akushon_interfaces/srv/run_action.hpp"
#include "nlohmann/json.hpp"
#include "rclcpp/rclcpp.hpp"
#include "tachimawari/joint/model/joint.hpp"
#include "tachimawari_interfaces/msg/set_joints.hpp"

using namespace std::chrono_literals;

namespace akushon
{

std::string ActionNode::get_node_prefix()
{
  return "action";
}

std::string ActionNode::run_action_service()
{
  return get_node_prefix() + "/run_action";
}

ActionNode::ActionNode(
  rclcpp::Node::SharedPtr node, std::shared_ptr<ActionManager> action_manager)
: node(node), action_manager(action_manager), now(node->now().seconds()),
  initial_pose(Pose("initial_pose"))
{
  current_joints_subscriber = node->create_subscription<tachimawari_interfaces::msg::CurrentJoints>(
    "/joint/current_joints", 10,
    [this](const tachimawari_interfaces::msg::CurrentJoints::SharedPtr message) {
      {
        using tachimawari::joint::Joint;
        std::vector<Joint> current_joints;

        for (const auto & joint : message->joints) {
          current_joints.push_back(Joint(joint.id, joint.position));
        }

        this->initial_pose.set_joints(current_joints);
      }
    }
  );

  set_joints_publisher = node->create_publisher<tachimawari_interfaces::msg::SetJoints>(
    "/joint/set_joints", 10);

  {
    using akushon_interfaces::srv::RunAction;
    run_action_server = node->create_service<RunAction>(
      run_action_service(),
      [this](std::shared_ptr<RunAction::Request> request,
      std::shared_ptr<RunAction::Response> response) {
        rclcpp::Rate rcl_rate(8ms);

        bool is_ready = false;

        if (request->control_type == RUN_ACTION_BY_NAME) {
          is_ready = start(request->action_name);
        } else {
          nlohmann::json action_data = nlohmann::json::parse(request->json);
          Action action = this->action_manager->load_action(action_data, request->action_name);

          is_ready = start(action);
        }

        if (is_ready) {
          while (rclcpp::ok()) {
            rcl_rate.sleep();

            double time = this->node->now().seconds() - this->now;
            if (!update(time * 1000)) {
              break;
            }
          }
        }

        if (rclcpp::ok()) {
          response->status = is_ready ? "SUCCEEDED" : "FAILED";
        }
      }
    );
  }
}

bool ActionNode::start(const std::string & action_name)
{
  Pose pose = this->initial_pose;

  if (!pose.get_joints().empty()) {
    action_manager->start(action_name, pose);
  } else {
    return false;
  }

  return true;
}

bool ActionNode::start(const Action & action)
{
  Pose pose = this->initial_pose;

  if (!pose.get_joints().empty()) {
    action_manager->start(action, pose);
  } else {
    return false;
  }

  return true;
}

bool ActionNode::update(int time)
{
  action_manager->process(time);

  if (action_manager->is_playing()) {
    publish_joints();

    return true;
  }

  return false;
}

void ActionNode::publish_joints()
{
  auto joints_msg = tachimawari_interfaces::msg::SetJoints();

  const auto & joints = action_manager->get_joints();
  auto & joint_msgs = joints_msg.joints;

  joint_msgs.resize(joints.size());
  for (size_t i = 0; i < joints.size() && i < joint_msgs.size(); ++i) {
    joint_msgs[i].id = joints[i].get_id();
    joint_msgs[i].position = joints[i].get_position();
  }

  set_joints_publisher->publish(joints_msg);
}

}  // namespace akushon
