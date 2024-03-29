/*
OBS Template Match Beep
Copyright (C) 2022 - 2023 Janne Pitkänen <acebanzkux@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program. If not, see <https://www.gnu.org/licenses/>
*/

#ifndef CUSTOMBEEPSETTINGS_H
#define CUSTOMBEEPSETTINGS_H

#ifndef MSEC_TO_SEC
#define MSEC_TO_SEC 0.001
#endif

#include <util/base.h>
#include <obs-data.h>
#include <QtWidgets>

#define SETTING_EVENT_ARRAY "event_array"

#define SETTING_EVENT_TYPE "event_type"
#define SETTING_EVENT_LENGTH "event_length"
#define SETTING_EVENT_FREQUENCY "event_frequency"

class MouseWheelWidgetAdjustmentGuard : public QObject {
public:
	explicit MouseWheelWidgetAdjustmentGuard(QObject *parent);

protected:
	bool eventFilter(QObject *o, QEvent *e) override;
};

enum class EventType { Beep, Wait };

struct Event {
	EventType type;
	int length;
	int frequency;
};
class CustomBeepSettings;

class ArrayItemWidget : public QWidget {
	Q_OBJECT
public:
	ArrayItemWidget(obs_data_t *item, CustomBeepSettings *settings, QWidget *parent = nullptr);
	ArrayItemWidget(CustomBeepSettings *settings, QWidget *parent = nullptr);
	~ArrayItemWidget();

private slots:
	void currentTypeChanged(int index);
	void currentLengthChanged(int value);
	void currentFrequencyChanged(int value);
	void removeClicked();

private:
	CustomBeepSettings *m_Settings;

	QHBoxLayout *m_Layout;
	QComboBox *m_Type;
	QSpinBox *m_Length;
	QSpinBox *m_Frequency;
	QPushButton *m_Button;
};

class CustomBeepSettings : public QObject {
	Q_OBJECT
public:
	CustomBeepSettings(QObject *parent = nullptr);
	~CustomBeepSettings();

	void CreateOBSSettings(obs_data_t *settings);
	void LoadSettings();

	void CreateSettingsWindow();
	void ShowSettingsWindow();

	void DeleteArrayItem(ArrayItemWidget *widget);

	void ChangedArrayItem(ArrayItemWidget *widget, const char *name, int value);

	std::vector<Event> GetEvents();

private slots:
	void WindowClosed(int result);

private:
	void addNewEvent();

	obs_data_t *CreateArrayItem(EventType type);

	void SetArrayItemType(obs_data_t *item, EventType type);

	obs_data_array_t *m_Settings;

	QPushButton *m_Button;
	QVBoxLayout *m_List;
	QVBoxLayout *m_MainLayout;
	QScrollArea *m_ScrollArea;
	QWidget *m_TechArea;
	QDialog *m_Window;
};
#endif // !CUSTOMBEEPSETTINGS_H