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
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "akushon/action/node/action_node.hpp"

#include "akushon/action/node/action_manager.hpp"
#include "akushon/action/model/action_name.hpp"
#include "akushon/action/model/pose.hpp"
#include "rclcpp/rclcpp.hpp"
#include "tachimawari/joint/model/joint.hpp"
#include "tachimawari_interfaces/msg/set_joints.hpp"
#include "tachimawari_interfaces/srv/get_joints.hpp"

using namespace std::chrono_literals;

namespace akushon
{

ActionNode::ActionNode(
  rclcpp::Node::SharedPtr node, std::shared_ptr<ActionManager> action_manager)
: node(node), action_manager(action_manager), status(READY)
{
  set_joints_publisher = node->create_publisher<tachimawari_interfaces::msg::SetJoints>(
    "/joint/set_joints", 10);

  get_joints_client = node->create_client<tachimawari_interfaces::srv::GetJoints>(
    "/joint/get_joints");
}

bool ActionNode::is_action_exist(int action_id) const
{
  return action_manager->get_action(action_id).get_name().empty();
}

bool ActionNode::is_action_exist(const std::string & action_name) const
{
  return is_action_exist(ActionName::map.at(action_name));
}

int ActionNode::get_status() const
{
  return status;
}

bool ActionNode::start(const std::string & action_name)
{
  return start(ActionName::map.at(action_name));
}

bool ActionNode::start(int action_id)
{
  while (!get_joints_client->wait_for_service(1s)) {
    if (rclcpp::ok()) {
      // service not available, waiting again...
    } else {
      // Interrupted while waiting for the service. Exiting.
      return false;
    }
  }

  auto result = get_joints_client->async_send_request(
    std::make_shared<tachimawari_interfaces::srv::GetJoints::Request>());

  if (rclcpp::spin_until_future_complete(node, result) ==
    rclcpp::FutureReturnCode::SUCCESS)
  {
    Pose pose("initial_pose");
    std::vector<tachimawari::joint::Joint> joints;

    for (const auto & joint : result.get()->joints) {
      joints.push_back(
        tachimawari::joint::Joint(joint.id, joint.position));
    }
    pose.set_joints(joints);

    action_manager->start(action_id, pose);
    status = PLAYING;
  } else {
    // Failed to call service
    return false;
  }

  return true;
}

void ActionNode::process(int time)
{
  action_manager->process(time);

  if (action_manager->is_playing()) {
    publish_joints();
  } else {
    status = READY;
  }
}

std::string ActionNode::get_node_prefix() const
{
  return "action";
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