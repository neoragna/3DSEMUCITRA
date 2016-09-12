#include <qcheckbox.h>
#include <QComboBox>
#include <QLineEdit>
#include <QTableWidgetItem>

#include "cheat_gui.h"
#include "core/loader/ncch.h"
#include "ui_cheat_gui.h"

CheatDialog::CheatDialog(QWidget *parent) :
    QDialog(parent), ui(new Ui::CheatDialog) {

    //Setup gui control settings
    ui->setupUi(this);
    setWindowFlags(Qt::Dialog | Qt::MSWindowsFixedSizeDialogHint);
    ui->tableCheats->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->tableCheats->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableCheats->setColumnWidth(0, 30);
    ui->tableCheats->setColumnWidth(2, 85);
    ui->tableCheats->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    ui->tableCheats->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    ui->tableCheats->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
    ui->textDetails->setEnabled(false);
    ui->textNotes->setEnabled(false);
    char buffer[50];
    auto a = sprintf(buffer, "%016llX", Loader::program_id);
    auto game_id = std::string(buffer);
    ui->labelTitle->setText("Title ID: " + QString::fromStdString(game_id));

    connect(ui->buttonClose, SIGNAL(released()), this, SLOT(OnCancel()));
    connect(ui->buttonNewCheat, SIGNAL(released()), this, SLOT(OnAddCheat()));
    connect(ui->buttonSave, SIGNAL(released()), this, SLOT(OnSave()));
    connect(ui->buttonDelete, SIGNAL(released()), this, SLOT(OnDelete()));
    connect(ui->tableCheats, SIGNAL(cellClicked(int, int)), this, SLOT(OnRowSelected(int, int)));
    connect(ui->textDetails, SIGNAL(textChanged()), this, SLOT(OnDetailsChanged()));
    connect(ui->textNotes, SIGNAL(textChanged()), this, SLOT(OnNotesChanged()));

    LoadCheats();
}

CheatDialog::~CheatDialog() {
    delete ui;
}

void CheatDialog::LoadCheats() {
    cheats = CheatEngine::CheatEngine::ReadFileContents();

    ui->tableCheats->setRowCount(cheats.size());

    for (int i = 0; i < cheats.size(); i++) {
        auto enabled = new QCheckBox("");
        enabled->setCheckState(cheats[i]->enabled ? Qt::CheckState::Checked : Qt::CheckState::Unchecked);
        enabled->setStyleSheet("margin-left:7px;");
        ui->tableCheats->setItem(i, 0, new QTableWidgetItem(""));
        ui->tableCheats->setCellWidget(i, 0, enabled);
        ui->tableCheats->setItem(i, 1, new QTableWidgetItem(QString::fromStdString(cheats[i]->name)));
        ui->tableCheats->setItem(i, 2, new QTableWidgetItem(QString::fromStdString(cheats[i]->type)));
        enabled->setProperty("row", i);
        connect(enabled, SIGNAL(stateChanged(int)), this, SLOT(OnCheckChanged(int)));
    }
}

void CheatDialog::OnSave() {
    CheatEngine::CheatEngine::Save(cheats);
    CheatCore::RefreshCheats();
    this->close();
}

void CheatDialog::OnCancel() {
    this->close();
}

void CheatDialog::OnRowSelected(int row, int column) {
    selection_changing = true;
    if (row == -1) {
        ui->textNotes->setPlainText("");
        ui->textDetails->setPlainText("");
        current_row = -1;
        selection_changing = false;
        ui->textDetails->setEnabled(false);
        ui->textNotes->setEnabled(false);
        return;
    }

    ui->textDetails->setEnabled(true);
    ui->textNotes->setEnabled(true);
    auto current_cheat = cheats[row];
    ui->textNotes->setPlainText(QString::fromStdString(Common::Join(current_cheat->notes, "\n")));

    std::vector<std::string> details;
    for (auto& line : current_cheat->cheat_lines)
        details.push_back(line.cheat_line);
    ui->textDetails->setPlainText(QString::fromStdString(Common::Join(details, "\n")));

    current_row = row;

    selection_changing = false;
}

void CheatDialog::OnNotesChanged() {
    if (selection_changing)
        return;
    auto notes = ui->textNotes->toPlainText();
    Common::SplitString(notes.toStdString(), '\n', cheats[current_row]->notes);
}

void CheatDialog::OnDetailsChanged() {
    if (selection_changing)
        return;
    auto details = ui->textDetails->toPlainText();
    std::vector<std::string> detail_lines;
    Common::SplitString(details.toStdString(), '\n', detail_lines);
    cheats[current_row]->cheat_lines.clear();
    for (auto& line : detail_lines) {
        cheats[current_row]->cheat_lines.push_back(CheatEngine::CheatLine(line));
    }
}

void CheatDialog::OnCheckChanged(int state) {
    QCheckBox* checkbox = qobject_cast<QCheckBox*>(sender());
    int row = static_cast<int>(checkbox->property("row").toInt());
    cheats[row]->enabled = state;
}

void CheatDialog::OnDelete() {
    QItemSelectionModel* selectionModel = ui->tableCheats->selectionModel();
    QModelIndexList selected = selectionModel->selectedRows();
    std::vector<int> rows;
    for (int i = selected.count() - 1; i >= 0; i--) {
        QModelIndex index = selected.at(i);
        int row = index.row();
        cheats.erase(cheats.begin() + row);
        rows.push_back(row);
    }
    for (int i = 0; i < rows.size(); i++)
        ui->tableCheats->removeRow(rows[i]);

    ui->tableCheats->clearSelection();
    OnRowSelected(-1, -1);
}

void CheatDialog::OnAddCheat() {
    QDialogEx* dialog = new QDialogEx(this);

    dialog->exec();

    auto result = dialog->return_value;
    if (result == nullptr)
        return;
    cheats.push_back(result);
    int i = cheats.size() - 1;
    auto enabled = new QCheckBox("");
    ui->tableCheats->setRowCount(cheats.size());
    enabled->setCheckState(Qt::CheckState::Unchecked);
    enabled->setStyleSheet("margin-left:7px;");
    ui->tableCheats->setItem(i, 0, new QTableWidgetItem(""));
    ui->tableCheats->setCellWidget(i, 0, enabled);
    ui->tableCheats->setItem(i, 1, new QTableWidgetItem(QString::fromStdString(cheats[i]->name)));
    ui->tableCheats->setItem(i, 2, new QTableWidgetItem(QString::fromStdString(cheats[i]->type)));
    enabled->setProperty("row", i);
    connect(enabled, SIGNAL(stateChanged(int)), this, SLOT(OnCheckChanged(int)));
    ui->tableCheats->selectRow(i);
    OnRowSelected(i, 0);
    delete dialog;
}
QDialogEx::QDialogEx(QWidget *parent) :
    QDialog(parent), ui(this) {
    this->resize(250, 150);
    this->setSizeGripEnabled(false);
    auto mainLayout = new QVBoxLayout(this);

    QHBoxLayout* panel = new QHBoxLayout();
    nameblock = new QLineEdit();
    QLabel* nameLabel = new QLabel();
    nameLabel->setText("Name: ");
    panel->addWidget(nameLabel);
    panel->addWidget(nameblock);

    QHBoxLayout* panel2 = new QHBoxLayout();
    auto typeLabel = new QLabel();
    typeLabel->setText("Type: ");
    typeSelect = new QComboBox();
    typeSelect->addItem("Gateway", 0);
    panel2->addWidget(typeLabel);
    panel2->addWidget(typeSelect);

    QHBoxLayout* panel3 = new QHBoxLayout();
    auto buttonOk = new QPushButton();
    buttonOk->setText("Ok");
    auto buttonCancel = new QPushButton();
    buttonCancel->setText("Cancel");
    connect(buttonOk, &QPushButton::released, this, [=]() {
        auto name = nameblock->text().toStdString();
        if (typeSelect->currentIndex() == 0 && Common::Trim(name).length() > 0) {
            return_value = std::make_shared<CheatEngine::GatewayCheat>(std::vector<CheatEngine::CheatLine>(), std::vector<std::string>(), false, nameblock->text().toStdString());
        }
        ui->close();
    });
    connect(buttonCancel, &QPushButton::released, this, [=]() {ui->close(); });

    panel3->addWidget(buttonOk);
    panel3->addWidget(buttonCancel);
    mainLayout->addLayout(panel);
    mainLayout->addLayout(panel2);
    mainLayout->addLayout(panel3);
}
QDialogEx::~QDialogEx() {
}