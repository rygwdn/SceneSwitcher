#pragma once
#include "midi-helpers.hpp"

#include <QDialog>
#include <QTableWidget>
#include <QComboBox>
#include <QPushButton>
#include <QTimer>

namespace advss {

enum class MidiEndpointMode {
	NONE = 0,
	INPUT = 1,
	OUTPUT = 2,
	BOTH = 3
};

class MidiEndpointSettings {
public:
	MidiEndpointSettings() = default;
	MidiEndpointSettings(const std::string &name, MidiEndpointMode mode)
		: _name(name), _mode(mode)
	{
	}

	void Save(obs_data_t *obj) const;
	void Load(obs_data_t *obj);

	std::string _name;
	MidiEndpointMode _mode = MidiEndpointMode::NONE;
};

class MidiSettingsDialog : public QDialog {
	Q_OBJECT

public:
	MidiSettingsDialog(QWidget *parent = nullptr);
	~MidiSettingsDialog();

	void UpdateDeviceList();
	void LoadSettings();
	void SaveSettings();

private slots:
	void RefreshDeviceList();
	void OnModeChanged(int row, int column);
	void OnApplyClicked();
	void OnOkClicked();
	void OnCancelClicked();

private:
	void SetupTable();
	void PopulateTable();
	MidiEndpointMode GetModeForDevice(const std::string &name) const;
	void SetModeForDevice(const std::string &name, MidiEndpointMode mode);
	void ApplySettings();

	QTableWidget *_table;
	QPushButton *_refreshButton;
	QPushButton *_applyButton;
	QPushButton *_okButton;
	QPushButton *_cancelButton;
	QTimer *_refreshTimer;

	std::vector<MidiEndpointSettings> _settings;
	bool _settingsChanged = false;
};

} // namespace advss
