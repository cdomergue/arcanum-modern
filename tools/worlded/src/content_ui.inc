    bool selectedActor()const {if(!building)return false;auto ids=selectedIds();for(auto& o:building->objects)if(ids.count(objectKey(o)))return o.type==15 || o.type==16;return false;}
    QStringList contentPaths(const QString& suffix) const {
        QSet<QString> paths;
        if(installation)for(const auto& entry:installation->resources()){auto path=utf8(entry.first);if(path.endsWith(suffix,Qt::CaseInsensitive))paths.insert(path);}
        if(building)for(const auto& entry:building->content){auto path=utf8(entry.first);if(path.endsWith(suffix,Qt::CaseInsensitive))paths.insert(path);}
        auto result=paths.values();result.sort();return result;
    }
    Bytes contentRead(const QString& path) const {
        if(building){auto found=building->content.find(path.toStdString());if(found!=building->content.end())return found->second;}
        return installation->read(path.toStdString());
    }
    static unsigned contentNumber(const QString& path){return QRegularExpression("^([0-9]+)").match(QFileInfo(path).baseName()).captured(1).toUInt();}
    void createContent(bool dialogue) {
        if(!editing()){QMessageBox::information(this,"Project required","Open an editable zone first.");return;}
        auto scripts=contentPaths(".scr"),dialogs=contentPaths(".dlg");QSet<unsigned> used;for(auto p:scripts+dialogs)used.insert(contentNumber(p));
        unsigned next=40000;while(used.contains(next))++next;
        QDialog dialog(this);dialog.setWindowTitle(dialogue?"New dialogue":"New script");auto form=new QFormLayout(&dialog);QSpinBox id;id.setRange(1,999999);id.setValue(next);QLineEdit name("new_resource");form->addRow("Script / dialogue ID",&id);form->addRow("Name (a–z, 0–9, _)",&name);QLabel error;error.setWordWrap(true);form->addRow(&error);QDialogButtonBox buttons(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);form->addRow(&buttons);
        connect(&buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);connect(&buttons,&QDialogButtonBox::accepted,&dialog,[&]{if(used.contains(id.value())){error.setText("This ID is already used. Choose another.");return;}if(!QRegularExpression("^[a-z][a-z0-9_]{0,63}$").match(name.text()).hasMatch()){error.setText("Use a lowercase name starting with a letter.");return;}dialog.accept();});
        if(dialog.exec()!=QDialog::Accepted)return;
        editContent(QString(dialogue?"dlg/%1_%2.dlg":"scr/%1_%2.scr").arg(id.value()).arg(name.text()),true);
    }
    void contentLibrary(){
        if(!installation)return;
        QDialog dialog(this);dialog.setWindowTitle("Scripts & dialogues");dialog.resize(760,640);auto layout=new QVBoxLayout(&dialog);QLineEdit search;search.setPlaceholderText("Search by ID or filename");layout->addWidget(&search);QListWidget list;layout->addWidget(&list,1);
        auto refresh=[&]{list.clear();for(auto path:contentPaths(".scr")+contentPaths(".dlg")){if(!path.contains(search.text(),Qt::CaseInsensitive))continue;auto item=new QListWidgetItem(path,&list);item->setData(Qt::UserRole,path);if(building && building->content.count(path.toStdString()))item->setText(path+"  [project]");}};refresh();connect(&search,&QLineEdit::textChanged,&dialog,[&]{refresh();});
        QHBoxLayout actions;QPushButton script("New script…"),dlg("New dialogue…"),edit("Open"),close("Close");actions.addWidget(&script);actions.addWidget(&dlg);actions.addStretch();actions.addWidget(&edit);actions.addWidget(&close);layout->addLayout(&actions);
        connect(&script,&QPushButton::clicked,&dialog,[&]{createContent(false);refresh();});connect(&dlg,&QPushButton::clicked,&dialog,[&]{createContent(true);refresh();});auto open=[&]{if(list.currentItem()){editContent(list.currentItem()->data(Qt::UserRole).toString());refresh();}};connect(&edit,&QPushButton::clicked,&dialog,open);connect(&list,&QListWidget::itemDoubleClicked,&dialog,[&]{open();});connect(&close,&QPushButton::clicked,&dialog,&QDialog::accept);dialog.exec();
    }
    void showScript(unsigned id){for(auto path:contentPaths(".scr"))if(contentNumber(path)==id){editContent(path);return;}QMessageBox::information(this,"Script not found",QString("No script matches ID %1. Use File → New script to create one.").arg(id));}
    void editContent(const QString& path,bool fresh=false){
        if(!installation)return;
        bool nestedObject=dynamic_cast<ActorEditor*>(QApplication::activeModalWidget())!=nullptr;
        try{
            bool dlg=path.endsWith(".dlg");auto stem=QFileInfo(path).completeBaseName();QString sourcePath="script-sources/"+stem+".wscript",scrPath="scr/"+stem+".scr",dlgPath="dlg/"+stem+".dlg";
            bool modern=fresh || (building && building->content.count(sourcePath.toStdString()));Bytes original=fresh?empty_script():contentRead(path);
            ScriptDialog dialog(this);dialog.setWindowTitle(path+(fresh?" — new":""));dialog.resize(1080,760);auto layout=new QVBoxLayout(&dialog);auto info=new QLabel(dlg?"Dialogue text uses the game's Windows-1252 encoding. Conditions and effects use native Arcanum syntax.":modern?"Modern script → native Arcanum SCR. Ctrl+Space: completion. Up to 10 numeric locals; the engine limits execution to 1000 instructions per invocation.":"Native SCR editor. Preserve instruction numbers used by dialogues and other scripts. Ctrl+Space: completion.");info->setWordWrap(true);layout->addWidget(info);
            auto tabs=new QTabWidget;layout->addWidget(tabs,1);auto editor=new ScriptEditor;auto conversation=new DialogueEditor;auto listing=new QPlainTextEdit;listing->setReadOnly(true);bool dirty=fresh;
            if(dlg){conversation->load(fresh?new_dialogue():parse_dialogue(original));tabs->addTab(conversation,"Dialogue");conversation->changed=[&]{dirty=true;};delete editor;editor=nullptr;}else {delete conversation;conversation=nullptr;dialog.editor=editor;if(modern)editor->modernCompletion();editor->setPlainText(fresh?"// Write your script here.\nreturn;\n":modern?QString::fromUtf8(QByteArray::fromStdString(std::string(building->content.at(sourcePath.toStdString()).begin(),building->content.at(sourcePath.toStdString()).end()))):script_source(original));editor->document()->setModified(false);tabs->addTab(editor,modern?"Modern source":"Native source");tabs->addTab(listing,"Compiled instructions");connect(editor,&QPlainTextEdit::textChanged,&dialog,[&]{dirty=true;});}
            if(dlg)delete listing;
            auto attach=new QCheckBox("Attach to selected object on apply");auto event=new QComboBox;for(auto name:attachmentNames)event->addItem(name);int use=event->findText(dlg && selectedActor()?"SAP_DIALOG":"SAP_USE");if(use>=0)event->setCurrentIndex(use);auto selection=selectedIds();attach->setEnabled(!nestedObject && editing() && selection.size()==1);attach->setChecked(fresh && attach->isEnabled());auto row=new QHBoxLayout;row->addWidget(attach);row->addWidget(event);row->addStretch();layout->addLayout(row);
            auto feedback=new QLabel;feedback->setWordWrap(true);layout->addWidget(feedback);auto buttons=new QHBoxLayout;auto validate=new QPushButton("Validate"),apply=new QPushButton("Apply to project"),companion=new QPushButton(dlg?"Open companion script":"Open dialogue"),close=new QPushButton("Close");buttons->addWidget(validate);buttons->addWidget(companion);buttons->addStretch();buttons->addWidget(apply);buttons->addWidget(close);layout->addLayout(buttons);apply->setEnabled(editing());
            auto compile=[&]{if(dlg)return encode_dialogue(conversation->value());auto bytes=modern?compile_modern_script(editor->toPlainText(),original).bytes:compile_script(original,editor->toPlainText());listing->setPlainText(script_listing(bytes));return bytes;};
            if(!dlg){try{compile();}catch(const std::exception& e){feedback->setText(e.what());}}
            connect(attach,&QCheckBox::toggled,&dialog,[&]{dirty=true;});connect(event,&QComboBox::currentIndexChanged,&dialog,[&]{if(attach->isChecked())dirty=true;});
            connect(validate,&QPushButton::clicked,&dialog,[&]{try{compile();auto warnings=dlg?validate_dialogue(conversation->value()):QStringList{};feedback->setText(warnings.isEmpty()?"Valid structure. Runtime conditions and effects require testing in the game.":warnings.join('\n'));}catch(const std::exception& e){feedback->setText(e.what());}});
            connect(apply,&QPushButton::clicked,&dialog,[&]{try{auto bytes=compile();auto next=*building;next.content[path.toStdString()]=std::move(bytes);if(!dlg && modern){auto text=editor->toPlainText().toUtf8();next.content[sourcePath.toStdString()]=Bytes(text.begin(),text.end());}
                if(dlg && !contentPaths(".scr").contains(scrPath)){auto lines=conversation->value();auto first=std::find_if(lines.begin(),lines.end(),[](const auto& line){return line.fields[3].trimmed().isEmpty();});QString source=QString("dialog(%1);\nreturn;\n").arg(first->fields[0]);next.content[scrPath.toStdString()]=compile_modern_script(source).bytes;auto text=source.toUtf8();next.content[sourcePath.toStdString()]=Bytes(text.begin(),text.end());}
                if(attach->isChecked())for(auto& object:next.objects)if(selection.count(objectKey(object))){auto resolved=assets->effective(object);ObjectField field;if(auto f=resolved.field("SCRIPTS_IDX"))field=*f;else{field.name="SCRIPTS_IDX";field.kind="SCRIPT_ARRAY";}field.present=true;auto slot=unsigned(event->currentIndex());auto it=std::find_if(field.elements.begin(),field.elements.end(),[&](auto& e){return e.index==slot;});if(it==field.elements.end()){field.elements.push_back({slot,Bytes(12,0)});it=std::prev(field.elements.end());}auto id=contentNumber(path);for(int b=0;b<4;b++)it->data[8+b]=id>>(8*b);std::sort(field.elements.begin(),field.elements.end(),[](auto&a,auto&b){return a.index<b.index;});ActorEditor::put(object,std::move(field));}
                commitEdit(dlg?"Edit dialogue":"Edit script",next.objects,selection,nullptr,&next);dirty=false;if(editor)editor->document()->setModified(false);feedback->setText("Applied. Save the zone to write the project files, then create a new test copy.");
            }catch(const std::exception& e){feedback->setText(e.what());}});
            connect(companion,&QPushButton::clicked,&dialog,[&]{if(dirty){feedback->setText("Apply your changes before opening the companion resource.");return;}auto target=dlg?scrPath:dlgPath;if(contentPaths(dlg?".scr":".dlg").contains(target))editContent(target);else if(!dlg)editContent(target,true);});
            connect(close,&QPushButton::clicked,&dialog,&QDialog::reject);
            // Both source and structured dialogue edits receive the same discard protection.
            dialog.canClose=[&]{return !dirty || QMessageBox::question(&dialog,"Unsaved changes","Discard unapplied changes?")==QMessageBox::Yes;};dialog.exec();
        }catch(const std::exception& e){QMessageBox::warning(this,"Cannot open resource",utf8(e.what()));}
    }
