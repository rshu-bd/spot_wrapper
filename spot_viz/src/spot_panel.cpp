#include "spot_panel.hpp"

#include <algorithm>
#include <cmath>
#include <QFile>
#include <QUiLoader>
#include <QVBoxLayout>
#include <ros/package.h>
#include <std_srvs/Trigger.h>
#include <geometry_msgs/Pose.h>
#include <geometry_msgs/Twist.h>
#include <QDoubleValidator>
#include <tf/transform_datatypes.h>
#include <spot_msgs/SetVelocity.h>
#include <spot_msgs/LeaseArray.h>
#include <spot_msgs/EStopState.h>
#include <spot_msgs/SetSwingHeight.h>
#include <spot_msgs/SetLocomotion.h>
#include <string.h>


namespace spot_viz
{

    ControlPanel::ControlPanel(QWidget *parent) {
        std::string packagePath = ros::package::getPath("spot_viz") + "/resource/spot_control.ui";
        ROS_INFO("Getting ui file from package path %s", packagePath.c_str());
        QFile file(packagePath.c_str());
        file.open(QIODevice::ReadOnly);

        QUiLoader loader;
        QWidget* ui = loader.load(&file, parent);
        file.close();
        QVBoxLayout* topLayout = new QVBoxLayout();
        this->setLayout(topLayout);
        topLayout->addWidget(ui);
        haveLease = false;

        sitService_ = nh_.serviceClient<std_srvs::Trigger>("/spot/sit");
        standService_ = nh_.serviceClient<std_srvs::Trigger>("/spot/stand");
        claimLeaseService_ = nh_.serviceClient<std_srvs::Trigger>("/spot/claim");
        releaseLeaseService_ = nh_.serviceClient<std_srvs::Trigger>("/spot/release");
        powerOnService_ = nh_.serviceClient<std_srvs::Trigger>("/spot/power_on");
        powerOffService_ = nh_.serviceClient<std_srvs::Trigger>("spot/power_off");
        maxVelocityService_ = nh_.serviceClient<spot_msgs::SetVelocity>("/spot/velocity_limit");
        hardStopService_ = nh_.serviceClient<std_srvs::Trigger>("/spot/estop/hard");
        gentleStopService_ = nh_.serviceClient<std_srvs::Trigger>("/spot/estop/gentle");
        releaseStopService_ = nh_.serviceClient<std_srvs::Trigger>("/spot/estop/release");
        stopService_ = nh_.serviceClient<std_srvs::Trigger>("/spot/stop");
        gaitService_ = nh_.serviceClient<spot_msgs::SetLocomotion>("/spot/locomotion_mode");
        swingHeightService_ = nh_.serviceClient<spot_msgs::SetSwingHeight>("/spot/locomotion_mode");

        bodyPosePub_ = nh_.advertise<geometry_msgs::Pose>("/spot/body_pose", 1);

        leaseSub_ = nh_.subscribe("/spot/status/leases", 1, &ControlPanel::leaseCallback, this);
        estopSub_ = nh_.subscribe("/spot/status/estop", 1, &ControlPanel::estopCallback, this);
        mobilityParamsSub_ = nh_.subscribe("/spot/status/mobility_params", 1, &ControlPanel::mobilityParamsCallback, this);
        batterySub_ = nh_.subscribe("/spot/status/battery_states", 1, &ControlPanel::batteryCallback, this);
        powerSub_ = nh_.subscribe("/spot/status/power_state", 1, &ControlPanel::powerCallback, this);

        claimLeaseButton = this->findChild<QPushButton*>("claimLeaseButton");
        releaseLeaseButton = this->findChild<QPushButton*>("releaseLeaseButton");
        powerOnButton = this->findChild<QPushButton*>("powerOnButton");
        powerOffButton = this->findChild<QPushButton*>("powerOffButton");
        standButton = this->findChild<QPushButton*>("standButton");
        sitButton = this->findChild<QPushButton*>("sitButton");
        setBodyPoseButton = this->findChild<QPushButton*>("setBodyPoseButton");
        setMaxVelButton = this->findChild<QPushButton*>("setMaxVelButton");
        statusLabel = this->findChild<QLabel*>("statusLabel");
        estimatedRuntimeLabel = this->findChild<QLabel*>("estimatedRuntimeLabel");
        batteryStateLabel = this->findChild<QLabel*>("batteryStateLabel");
        motorStateLabel = this->findChild<QLabel*>("motorStateLabel");
        batteryTempLabel = this->findChild<QLabel*>("batteryTempLabel");
        setGaitButton = this->findChild<QPushButton*>("setGaitButton");
        setSwingHeightButton = this->findChild<QPushButton*>("setSwingHeightButton");
        gaitComboBox = this->findChild<QComboBox*>("gaitComboBox");
        swingHeightComboBox = this->findChild<QComboBox*>("swingHeightComboBox");

        gaitComboBox->insertItem(spot_msgs::SetLocomotion::Request::HINT_AMBLE, "Amble");
        gaitComboBox->insertItem(spot_msgs::SetLocomotion::Request::HINT_AUTO, "Auto");
        gaitComboBox->insertItem(spot_msgs::SetLocomotion::Request::HINT_CRAWL, "Crawl");
        gaitComboBox->insertItem(spot_msgs::SetLocomotion::Request::HINT_HOP, "Hop");
        gaitComboBox->insertItem(spot_msgs::SetLocomotion::Request::HINT_JOG, "Jog");
        gaitComboBox->insertItem(spot_msgs::SetLocomotion::Request::HINT_SPEED_SELECT_AMBLE, "Speed sel amble");
        gaitComboBox->insertItem(spot_msgs::SetLocomotion::Request::HINT_SPEED_SELECT_CRAWL, "Speed sel crawl");
        gaitComboBox->insertItem(spot_msgs::SetLocomotion::Request::HINT_SPEED_SELECT_TROT, "Speed sel trot");
        gaitComboBox->insertItem(spot_msgs::SetLocomotion::Request::HINT_TROT, "Trot");

        swingHeightComboBox->insertItem(spot_msgs::SetSwingHeight::Request::SWING_HEIGHT_LOW, "Low");
        swingHeightComboBox->insertItem(spot_msgs::SetSwingHeight::Request::SWING_HEIGHT_MEDIUM, "Medium");
        swingHeightComboBox->insertItem(spot_msgs::SetSwingHeight::Request::SWING_HEIGHT_HIGH, "High");

        stopButton = this->findChild<QPushButton*>("stopButton");
        QPalette pal = stopButton->palette();
        pal.setColor(QPalette::Button, QColor(255, 165, 0));
        stopButton->setAutoFillBackground(true);
        stopButton->setPalette(pal);
        stopButton->update();

        gentleStopButton = this->findChild<QPushButton*>("gentleStopButton");
        pal = gentleStopButton->palette();
        pal.setColor(QPalette::Button, QColor(255, 0, 255));
        gentleStopButton->setAutoFillBackground(true);
        gentleStopButton->setPalette(pal);
        gentleStopButton->update();

        hardStopButton = this->findChild<QPushButton*>("hardStopButton");
        hardStopButton->setText(QString::fromUtf8("\u26A0 Kill Motors"));
        pal = hardStopButton->palette();
        pal.setColor(QPalette::Button, QColor(255, 0, 0));
        hardStopButton->setAutoFillBackground(true);
        hardStopButton->setPalette(pal);
        hardStopButton->update();

        releaseStopButton = this->findChild<QPushButton*>("releaseStopButton");
        pal = releaseStopButton->palette();
        pal.setColor(QPalette::Button, QColor(0, 255, 0));
        releaseStopButton->setAutoFillBackground(true);
        releaseStopButton->setPalette(pal);
        releaseStopButton->update();


        double linearVelocityLimit = 2;
        linearXSpin = this->findChild<QDoubleSpinBox*>("linearXSpin");
        linearXLabel = this->findChild<QLabel*>("linearXLabel");
        updateLabelTextWithLimit(linearXLabel, 0, linearVelocityLimit);
        linearXSpin->setMaximum(linearVelocityLimit);
        linearXSpin->setMinimum(0);

        linearYSpin = this->findChild<QDoubleSpinBox*>("linearYSpin");
        linearYLabel = this->findChild<QLabel*>("linearYLabel");
        updateLabelTextWithLimit(linearYLabel, 0, linearVelocityLimit);
        linearYSpin->setMaximum(linearVelocityLimit);
        linearYSpin->setMinimum(0);

        angularZSpin = this->findChild<QDoubleSpinBox*>("angularZSpin");
        angularZLabel = this->findChild<QLabel*>("angularZLabel");
        updateLabelTextWithLimit(angularZLabel, 0, linearVelocityLimit);
        angularZSpin->setMaximum(linearVelocityLimit);
        angularZSpin->setMinimum(0);

        double bodyHeightLimit = 0.15;
        bodyHeightSpin = this->findChild<QDoubleSpinBox*>("bodyHeightSpin");
        bodyHeightLabel = this->findChild<QLabel*>("bodyHeightLabel");
        updateLabelTextWithLimit(bodyHeightLabel, -bodyHeightLimit, bodyHeightLimit);
        bodyHeightSpin->setMaximum(bodyHeightLimit);
        bodyHeightSpin->setMinimum(-bodyHeightLimit);

        double rollLimit = 20;
        rollSpin = this->findChild<QDoubleSpinBox*>("rollSpin");
        rollLabel = this->findChild<QLabel*>("rollLabel");
        updateLabelTextWithLimit(rollLabel, -rollLimit, rollLimit);
        rollSpin->setMaximum(rollLimit);
        rollSpin->setMinimum(-rollLimit);

        double pitchLimit = 30;
        pitchSpin = this->findChild<QDoubleSpinBox*>("pitchSpin");
        pitchLabel = this->findChild<QLabel*>("pitchLabel");
        updateLabelTextWithLimit(pitchLabel, -pitchLimit, pitchLimit);
        pitchSpin->setMaximum(pitchLimit);
        pitchSpin->setMinimum(-pitchLimit);

        double yawLimit = 30;
        yawSpin = this->findChild<QDoubleSpinBox*>("yawSpin");
        yawLabel = this->findChild<QLabel*>("yawLabel");
        updateLabelTextWithLimit(yawLabel, -yawLimit, yawLimit);
        yawSpin->setMaximum(yawLimit);
        yawSpin->setMinimum(-yawLimit);

        connect(claimLeaseButton, SIGNAL(clicked()), this, SLOT(claimLease()));
        connect(releaseLeaseButton, SIGNAL(clicked()), this, SLOT(releaseLease()));
        connect(powerOnButton, SIGNAL(clicked()), this, SLOT(powerOn()));
        connect(powerOffButton, SIGNAL(clicked()), this, SLOT(powerOff()));
        connect(sitButton, SIGNAL(clicked()), this, SLOT(sit()));
        connect(standButton, SIGNAL(clicked()), this, SLOT(stand()));
        connect(setBodyPoseButton, SIGNAL(clicked()), this, SLOT(sendBodyPose()));
        connect(setMaxVelButton, SIGNAL(clicked()), this, SLOT(setMaxVel()));
        connect(releaseStopButton, SIGNAL(clicked()), this, SLOT(releaseStop()));
        connect(hardStopButton, SIGNAL(clicked()), this, SLOT(hardStop()));
        connect(gentleStopButton, SIGNAL(clicked()), this, SLOT(gentleStop()));
        connect(stopButton, SIGNAL(clicked()), this, SLOT(stop()));
        connect(setGaitButton, SIGNAL(clicked()), this, SLOT(setGait()));
        connect(setSwingHeightButton, SIGNAL(clicked()), this, SLOT(setSwingHeight()));

    }

    void ControlPanel::updateLabelTextWithLimit(QLabel* label, double limit_lower, double limit_upper) {
        int precision = 1;
        // Kind of hacky but default to_string returns 6 digit precision which is unnecessary
        std::string limit_lower_value = std::to_string(limit_lower).substr(0, std::to_string(limit_lower).find(".") + precision + 1);
        std::string limit_upper_value = std::to_string(limit_upper).substr(0, std::to_string(limit_upper).find(".") + precision + 1);
        std::string limit_range = " [" + limit_lower_value + ", " + limit_upper_value + "]";
        std::string current_text = label->text().toStdString();
        label->setText(QString((current_text + limit_range).c_str()));
    }

    void ControlPanel::setControlButtons() {
        claimLeaseButton->setEnabled(!haveLease);
        releaseLeaseButton->setEnabled(haveLease);
        powerOnButton->setEnabled(haveLease);
        powerOffButton->setEnabled(haveLease);
        sitButton->setEnabled(haveLease);
        standButton->setEnabled(haveLease);
        setBodyPoseButton->setEnabled(haveLease);
        setMaxVelButton->setEnabled(haveLease);
        releaseStopButton->setEnabled(haveLease && isEStopped);
        hardStopButton->setEnabled(haveLease);
        gentleStopButton->setEnabled(haveLease);
        stopButton->setEnabled(haveLease);
        setGaitButton->setEnabled(haveLease);
        setSwingHeightButton->setEnabled(haveLease);
    }

    bool ControlPanel::callTriggerService(ros::ServiceClient service, std::string serviceName) {
        std_srvs::Trigger req;
        std::string labelText = "Calling " + serviceName + " service";
        statusLabel->setText(QString(labelText.c_str()));
        if (service.call(req)) {
            if (req.response.success) {
                labelText = "Successfully called " + serviceName + " service";
                statusLabel->setText(QString(labelText.c_str()));
                return true;
            } else {
                labelText = serviceName + " service failed: " + req.response.message;
                statusLabel->setText(QString(labelText.c_str()));
                return false;
            }
        } else {
            labelText = "Failed to call " + serviceName + " service" + req.response.message;
            statusLabel->setText(QString(labelText.c_str()));
            return false;
        }
    }

    void ControlPanel::leaseCallback(const spot_msgs::LeaseArray::ConstPtr &leases) {
        // check to see if the body is already owned by the ROS node
        // the resource will be "body" and the lease_owner.client_name will begin with "ros_spot"
        // if the claim exists, treat this as a successful click of the Claim button
        // if the claim does not exist, treat this as a click of the Release button

        bool msg_has_lease = false;
        for (int i=leases->resources.size()-1; i>=0; i--) {
            const spot_msgs::LeaseResource &resource = leases->resources[i];
            bool right_resource = resource.resource.compare("body") == 0;
            bool owned_by_ros = resource.lease_owner.client_name.compare(0, 8, "ros_spot") == 0;

            if (right_resource && owned_by_ros) {
                msg_has_lease = true;
            }
        }

        if (msg_has_lease != haveLease) {
            haveLease = msg_has_lease;
            setControlButtons();
        }
    }

    void ControlPanel::estopCallback(const spot_msgs::EStopStateArray::ConstPtr &estops) {
        // Check to see if any of the estops is active, and set the state of the release
        // estop button accordingly if the state has changed
        bool msg_is_estopped = false;
        for (int i=estops->estop_states.size()-1; i>=0; i--) {
            const spot_msgs::EStopState &estop = estops->estop_states[i];
            if (estop.state == spot_msgs::EStopState::STATE_ESTOPPED) {
                msg_is_estopped = true;
                break;
            }
        }
        if (msg_is_estopped != isEStopped) {
            isEStopped = msg_is_estopped;
            setControlButtons();
        }
    }

    void ControlPanel::mobilityParamsCallback(const spot_msgs::MobilityParams::ConstPtr &params) {
        if (*params == _lastMobilityParams) {
            // If we don't check this, the user will never be able to modify values since they will constantly reset
            return;
        }

        linearXSpin->setValue(params->velocity_limit.linear.x);
        linearYSpin->setValue(params->velocity_limit.linear.y);
        angularZSpin->setValue(params->velocity_limit.angular.z);
        gaitComboBox->setCurrentIndex(params->locomotion_hint);
        swingHeightComboBox->setCurrentIndex(params->swing_height);

        _lastMobilityParams = *params;
    }

    void ControlPanel::batteryCallback(const spot_msgs::BatteryStateArray::ConstPtr &battery) {
        spot_msgs::BatteryState battState = battery->battery_states[0];
        std::string estRuntime = "Estimated runtime: " + std::to_string(battState.estimated_runtime.sec/60) + " min";
        estimatedRuntimeLabel->setText(QString(estRuntime.c_str()));

        auto temps = battState.temperatures;
        if (!temps.empty()) {
            auto minmax = std::minmax_element(temps.begin(), temps.end());
            float total = std::accumulate(temps.begin(), temps.end(), 0);
            // Don't care about float values here
            int tempMin = *minmax.first;
            int tempMax = *minmax.second;
            int tempAvg = total / temps.size();
            std::string battTemp = "Battery temp: min " + std::to_string(tempMin) + ", max " + std::to_string(tempMax) + ", avg " + std::to_string(tempAvg);
            batteryTempLabel->setText(QString(battTemp.c_str()));
        } else {
            batteryTempLabel->setText(QString("Battery temp: No battery"));
        }


        std::string status;
        switch (battState.status)
        {
        case spot_msgs::BatteryState::STATUS_UNKNOWN:
            status = "Unknown";
            break;
        case spot_msgs::BatteryState::STATUS_MISSING:
            status = "Missing";
            break;
        case spot_msgs::BatteryState::STATUS_CHARGING:
            status = "Charging";
            break;
        case spot_msgs::BatteryState::STATUS_DISCHARGING:
            status = "Discharging";
            break;
        case spot_msgs::BatteryState::STATUS_BOOTING:
            status = "Booting";
            break;
        default:
            status = "Invalid";
            break;
        }
        std::string battStatusStr;
        if (battState.status == spot_msgs::BatteryState::STATUS_CHARGING || battState.status == spot_msgs::BatteryState::STATUS_DISCHARGING) {
            // TODO: use std::format in c++20 rather than this nastiness
            std::stringstream stream;
            stream << std::fixed << std::setprecision(0) << battState.charge_percentage;
            std::string pct = stream.str() + "%";
            stream.str("");
            stream.clear();
            stream << std::fixed << std::setprecision(1) << battState.voltage;
            std::string volt = stream.str() + "V";
            stream.str("");
            stream.clear();
            stream << battState.current;
            std::string amp = stream.str() + "A";
            battStatusStr = "Battery state: " + status + ", " + pct + ", " + volt + ", " + amp;
        } else {
            battStatusStr = "Battery state: " + status;
        }
        batteryStateLabel->setText(QString(battStatusStr.c_str()));
    }

    void ControlPanel::powerCallback(const spot_msgs::PowerState::ConstPtr &power) {
        std::string state;
        switch (power->motor_power_state)
        {
        case spot_msgs::PowerState::STATE_POWERING_ON:
            state = "Powering on";
            powerOnButton->setEnabled(false);
            break;
        case spot_msgs::PowerState::STATE_POWERING_OFF:
            state = "Powering off";
            powerOffButton->setEnabled(false);
            break;
        case spot_msgs::PowerState::STATE_ON:
            state = "On";
            powerOnButton->setEnabled(false);
            powerOffButton->setEnabled(true);
            break;
        case spot_msgs::PowerState::STATE_OFF:
            state = "Off";
            powerOnButton->setEnabled(true);
            powerOffButton->setEnabled(false);
            break;
        case spot_msgs::PowerState::STATE_ERROR:
            state = "Error";
            break;
        case spot_msgs::PowerState::STATE_UNKNOWN:
            state = "Unknown";
            break;
        default:
            "Invalid";
        }
        std::string motorState = "Motor state: " + state;
        motorStateLabel->setText(QString(motorState.c_str()));
    }

    void ControlPanel::sit() {
        callTriggerService(sitService_, "sit");
    }

    void ControlPanel::stand() {
        callTriggerService(standService_, "stand");
    }

    void ControlPanel::powerOn() {
        callTriggerService(powerOnService_, "power on");
    }

    void ControlPanel::powerOff() {
        callTriggerService(powerOffService_, "power off");
    }

    void ControlPanel::claimLease() {
        if (callTriggerService(claimLeaseService_, "claim lease"))
            claimLeaseButton->setEnabled(false);
    }

    void ControlPanel::releaseLease() {
        if (callTriggerService(releaseLeaseService_, "release lease"))
            releaseLeaseButton->setEnabled(false);
    }

    void ControlPanel::stop() {
        callTriggerService(stopService_, "stop");
    }

    void ControlPanel::hardStop() {
        callTriggerService(hardStopService_, "hard stop");
    }

    void ControlPanel::gentleStop() {
        callTriggerService(gentleStopService_, "gentle stop");
    }

    void ControlPanel::releaseStop() {
        callTriggerService(releaseStopService_, "release stop");
    }

    void ControlPanel::setMaxVel() {
        spot_msgs::SetVelocity req;
        req.request.velocity_limit.angular.z = angularZSpin->value();
        req.request.velocity_limit.linear.x = linearXSpin->value();
        req.request.velocity_limit.linear.y = linearYSpin->value();

        std::string labelText = "Calling set velocity limit service";
        statusLabel->setText(QString(labelText.c_str()));
        if (maxVelocityService_.call(req)) {
            if (req.response.success) {
                labelText = "Successfully called set velocity limit service";
                statusLabel->setText(QString(labelText.c_str()));
            } else {
                labelText = "set velocity limit service failed: " + req.response.message;
                statusLabel->setText(QString(labelText.c_str()));
            }
        } else {
            labelText = "Failed to call set velocity limit service" + req.response.message;
            statusLabel->setText(QString(labelText.c_str()));
        }
    }

    void ControlPanel::sendBodyPose() {
        ROS_INFO("Sending body pose");
        tf::Quaternion q;
        q.setRPY(rollSpin->value() * M_PI / 180, pitchSpin->value() * M_PI / 180, yawSpin->value() * M_PI / 180);
        geometry_msgs::Pose p;
        p.position.z = bodyHeightSpin->value();
        p.orientation.x = q.getX();
        p.orientation.y = q.getY();
        p.orientation.z = q.getZ();
        p.orientation.w = q.getW();
        bodyPosePub_.publish(p);
    }

    void ControlPanel::setGait() {
        spot_msgs::SetLocomotion req;
        req.request.locomotion_mode = gaitComboBox->currentIndex();
        std::string labelText = "Calling set gait";
        if (gaitService_.call(req)) {
            if (req.response.success) {
                labelText = "Successfully called set gait service";
                statusLabel->setText(QString(labelText.c_str()));
            } else {
                labelText = "set gait failed: " + req.response.message;
                statusLabel->setText(QString(labelText.c_str()));
            }
        } else {
            labelText = "Failed to call gait service" + req.response.message;
            statusLabel->setText(QString(labelText.c_str()));
        }
    }

    void ControlPanel::setSwingHeight() {
        spot_msgs::SetSwingHeight req;
        req.request.swing_height = swingHeightComboBox->currentIndex();
        std::string labelText = "Calling set swing height";
        if (swingHeightService_.call(req)) {
            if (req.response.success) {
                labelText = "Successfully called set swing height service";
                statusLabel->setText(QString(labelText.c_str()));
            } else {
                labelText = "set swing height failed: " + req.response.message;
                statusLabel->setText(QString(labelText.c_str()));
            }
        } else {
            labelText = "Failed to swing height service" + req.response.message;
            statusLabel->setText(QString(labelText.c_str()));
        }
    }

    void ControlPanel::save(rviz::Config config) const
    {
        rviz::Panel::save(config);
    }

    // Load all configuration data for this panel from the given Config object.
    void ControlPanel::load(const rviz::Config &config)
    {
        rviz::Panel::load(config);
    }
} // end namespace spot_viz

#include <pluginlib/class_list_macros.h>
PLUGINLIB_EXPORT_CLASS(spot_viz::ControlPanel, rviz::Panel)
// END_TUTORIAL
