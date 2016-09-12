// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>

#include <QDialog>

#include "core/cheat_core.h"

class QComboBox;
class QLineEdit;
class QWidget;
namespace Ui {
    class CheatDialog;
    class QDialogEx;
}

class CheatDialog : public QDialog {
    Q_OBJECT

public:
    explicit CheatDialog(QWidget* parent = 0);
    ~CheatDialog();

private:
    Ui::CheatDialog* ui;
    int current_row = -1;
    bool selection_changing = false;
    std::vector<std::shared_ptr<CheatEngine::ICheat>> cheats;

    void LoadCheats();

    private slots:
    void OnAddCheat();
    void OnSave();
    void OnCancel();
    void OnRowSelected(int row, int column);
    void OnNotesChanged();
    void OnDetailsChanged();
    void OnCheckChanged(int state);
    void OnDelete();
};

class QDialogEx : public QDialog {
    Q_OBJECT
public:
    explicit QDialogEx(QWidget* parent = 0);
    ~QDialogEx();
    std::shared_ptr<CheatEngine::ICheat> return_value;
private:
    QDialogEx* ui;
    QLineEdit* nameblock;
    QComboBox* typeSelect;
};
