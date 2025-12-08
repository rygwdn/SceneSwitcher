#include "midi-settings.hpp"

#include <layout-helpers.hpp>
#include <log-helper.hpp>
#include <obs-module-helper.hpp>
#include <switcher-data.hpp>
#include <ui-helpers.hpp>

#include <QHeaderView>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QSet>
#include <QGroupBox>
#include <QAbstractItemView>

namespace advss {

void MidiEndpointSettings::Save(obs_data_t *obj) const
{
	obs_data_set_string(obj, "name", _name.c_str());
	obs_data_set_int(obj, "mode", static_cast<int>(_mode));
}

void MidiEndpointSettings::Load(obs_data_t *obj)
{
	_name = obs_data_get_string(obj, "name");
	_mode = static_cast<MidiEndpointMode>(obs_data_get_int(obj, "mode"));
}

MidiSettingsDialog::MidiSettingsDialog(QWidget *parent)
	: QDialog(parent),
	  _table(new QTableWidget(this)),
	  _refreshButton(new QPushButton(obs_module_text(
		  "AdvSceneSwitcher.midi.settings.refresh"), this)),
	  _applyButton(new QPushButton(obs_module_text(
		  "AdvSceneSwitcher.midi.settings.apply"), this)),
	  _okButton(new QPushButton(obs_module_text(
		  "AdvSceneSwitcher.midi.settings.ok"), this)),
	  _cancelButton(new QPushButton(obs_module_text(
		  "AdvSceneSwitcher.midi.settings.cancel"), this))
{
	setWindowTitle(obs_module_text(
		"AdvSceneSwitcher.midi.settings.title"));
	setMinimumSize(600, 400);

	SetupTable();

	_refreshButton->setToolTip(obs_module_text(
		"AdvSceneSwitcher.midi.settings.refresh.tooltip"));
	_applyButton->setToolTip(obs_module_text(
		"AdvSceneSwitcher.midi.settings.apply.tooltip"));

	connect(_refreshButton, &QPushButton::clicked, this,
		&MidiSettingsDialog::RefreshDeviceList);
	connect(_applyButton, &QPushButton::clicked, this,
		&MidiSettingsDialog::OnApplyClicked);
	connect(_okButton, &QPushButton::clicked, this,
		&MidiSettingsDialog::OnOkClicked);
	connect(_cancelButton, &QPushButton::clicked, this,
		&MidiSettingsDialog::OnCancelClicked);

	auto mainLayout = new QVBoxLayout(this);
	mainLayout->setContentsMargins(10, 10, 10, 10);

	auto infoLabel = new QLabel(obs_module_text(
		"AdvSceneSwitcher.midi.settings.description"), this);
	infoLabel->setWordWrap(true);
	mainLayout->addWidget(infoLabel);

	mainLayout->addWidget(_table);

	auto buttonLayout = new QHBoxLayout();
	buttonLayout->addWidget(_refreshButton);
	buttonLayout->addStretch();
	buttonLayout->addWidget(_applyButton);
	buttonLayout->addWidget(_okButton);
	buttonLayout->addWidget(_cancelButton);
	mainLayout->addLayout(buttonLayout);

	_refreshTimer = new QTimer(this);
	connect(_refreshTimer, &QTimer::timeout, this,
		&MidiSettingsDialog::RefreshDeviceList);
	_refreshTimer->start(2000); // Refresh every 2 seconds

	LoadSettings();
	UpdateDeviceList();
}

MidiSettingsDialog::~MidiSettingsDialog()
{
	_refreshTimer->stop();
}

void MidiSettingsDialog::SetupTable()
{
	_table->setColumnCount(3);
	_table->setHorizontalHeaderLabels(
		{QString(obs_module_text("AdvSceneSwitcher.midi.settings.device")),
		 QString(obs_module_text("AdvSceneSwitcher.midi.settings.input")),
		 QString(obs_module_text(
			 "AdvSceneSwitcher.midi.settings.output"))});

	_table->horizontalHeader()->setStretchLastSection(true);
	_table->setSelectionBehavior(QAbstractItemView::SelectRows);
	_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
	_table->setAlternatingRowColors(true);

	// Make input and output columns checkboxes
	_table->setItemDelegateForColumn(1, nullptr);
	_table->setItemDelegateForColumn(2, nullptr);

	// Connect cell changed signal to update mode (only once)
	connect(_table, &QTableWidget::cellChanged, this,
		&MidiSettingsDialog::OnModeChanged);
}

void MidiSettingsDialog::UpdateDeviceList()
{
	PopulateTable();
}

void MidiSettingsDialog::RefreshDeviceList()
{
	UpdateDeviceList();
}

void MidiSettingsDialog::PopulateTable()
{
	// Get all available MIDI devices
	QStringList inputDevices = GetInputDeviceNames();
	QStringList outputDevices = GetOutputDeviceNames();

	// Combine and deduplicate device names
	QSet<QString> allDevicesSet;
	for (const auto &dev : inputDevices) {
		allDevicesSet.insert(dev);
	}
	for (const auto &dev : outputDevices) {
		allDevicesSet.insert(dev);
	}

	QStringList deviceList = allDevicesSet.values();
	deviceList.sort();

	// Block signals during table population to prevent spurious OnModeChanged calls
	const QSignalBlocker blocker(_table);
	_table->setRowCount(deviceList.size());

	for (int i = 0; i < deviceList.size(); i++) {
		const QString &deviceName = deviceList[i];
		std::string deviceNameStd = deviceName.toStdString();

		// Device name column
		auto nameItem = new QTableWidgetItem(deviceName);
		nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);
		_table->setItem(i, 0, nameItem);

		// Check if device supports input
		bool hasInput = inputDevices.contains(deviceName);
		// Check if device supports output
		bool hasOutput = outputDevices.contains(deviceName);

		// Input checkbox
		auto inputItem = new QTableWidgetItem();
		inputItem->setFlags(inputItem->flags() | Qt::ItemIsUserCheckable);
		if (!hasInput) {
			inputItem->setFlags(inputItem->flags() &
					    ~Qt::ItemIsEnabled);
			inputItem->setToolTip(obs_module_text(
				"AdvSceneSwitcher.midi.settings.input.notAvailable"));
		}
		MidiEndpointMode mode = GetModeForDevice(deviceNameStd);
		bool inputEnabled = (mode == MidiEndpointMode::INPUT ||
				     mode == MidiEndpointMode::BOTH);
		inputItem->setCheckState(inputEnabled ? Qt::Checked : Qt::Unchecked);
		_table->setItem(i, 1, inputItem);

		// Output checkbox
		auto outputItem = new QTableWidgetItem();
		outputItem->setFlags(outputItem->flags() |
				     Qt::ItemIsUserCheckable);
		if (!hasOutput) {
			outputItem->setFlags(outputItem->flags() &
					     ~Qt::ItemIsEnabled);
			outputItem->setToolTip(obs_module_text(
				"AdvSceneSwitcher.midi.settings.output.notAvailable"));
		}
		bool outputEnabled = (mode == MidiEndpointMode::OUTPUT ||
				      mode == MidiEndpointMode::BOTH);
		outputItem->setCheckState(outputEnabled ? Qt::Checked : Qt::Unchecked);
		_table->setItem(i, 2, outputItem);
	}

	_table->resizeColumnsToContents();
	_table->horizontalHeader()->setStretchLastSection(true);
}

void MidiSettingsDialog::OnModeChanged(int row, int column)
{
	if (column == 0) {
		return; // Device name column, ignore
	}

	auto nameItem = _table->item(row, 0);
	if (!nameItem) {
		return;
	}

	std::string deviceName = nameItem->text().toStdString();
	auto inputItem = _table->item(row, 1);
	auto outputItem = _table->item(row, 2);

	if (!inputItem || !outputItem) {
		return;
	}

	bool inputChecked = inputItem->checkState() == Qt::Checked;
	bool outputChecked = outputItem->checkState() == Qt::Checked;

	MidiEndpointMode newMode;
	if (inputChecked && outputChecked) {
		newMode = MidiEndpointMode::BOTH;
	} else if (inputChecked) {
		newMode = MidiEndpointMode::INPUT;
	} else if (outputChecked) {
		newMode = MidiEndpointMode::OUTPUT;
	} else {
		newMode = MidiEndpointMode::NONE;
	}

	SetModeForDevice(deviceName, newMode);
	_settingsChanged = true;
}

MidiEndpointMode
MidiSettingsDialog::GetModeForDevice(const std::string &name) const
{
	for (const auto &setting : _settings) {
		if (setting._name == name) {
			return setting._mode;
		}
	}
	// Device not in settings - return BOTH to match runtime behavior
	// (IsMidiEndpointEnabled returns true for devices not in settings)
	// The actual enabled state depends on what the device supports
	QString qName = QString::fromStdString(name);
	QStringList inputDevices = GetInputDeviceNames();
	QStringList outputDevices = GetOutputDeviceNames();
	bool hasInput = inputDevices.contains(qName);
	bool hasOutput = outputDevices.contains(qName);
	if (hasInput && hasOutput) {
		return MidiEndpointMode::BOTH;
	} else if (hasInput) {
		return MidiEndpointMode::INPUT;
	} else if (hasOutput) {
		return MidiEndpointMode::OUTPUT;
	}
	return MidiEndpointMode::NONE;
}

void MidiSettingsDialog::SetModeForDevice(const std::string &name,
					   MidiEndpointMode mode)
{
	for (auto &setting : _settings) {
		if (setting._name == name) {
			setting._mode = mode;
			return;
		}
	}
	_settings.emplace_back(name, mode);
}

void MidiSettingsDialog::ApplySettings()
{
	// Close all currently open MIDI ports
	MidiDeviceInstance::ResetAllDevices();

	// Open all enabled MIDI endpoints (including newly enabled ones)
	// This handles devices that weren't previously in the devices map
	OpenEnabledMidiEndpoints();

	_settingsChanged = false;
}

void MidiSettingsDialog::OnApplyClicked()
{
	SaveSettings();
	ApplySettings();
	DisplayMessage(obs_module_text(
		"AdvSceneSwitcher.midi.settings.applied"));
}

void MidiSettingsDialog::OnOkClicked()
{
	SaveSettings();
	ApplySettings();
	accept();
}

void MidiSettingsDialog::OnCancelClicked()
{
	reject();
}

void MidiSettingsDialog::LoadSettings()
{
	_settings.clear();
	if (!switcher) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_settings = switcher->midiEndpointSettings;
}

void MidiSettingsDialog::SaveSettings()
{
	if (!switcher) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	switcher->midiEndpointSettings = _settings;
}

} // namespace advss
