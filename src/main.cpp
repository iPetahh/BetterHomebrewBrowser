#include "main.hpp"
#include "paf.hpp"
#include "pagemgr.hpp"
#include "common.hpp"
#include "utils.hpp"
#include "parser.hpp"
#include "configmgr.hpp"
#include "network.hpp"
#include "Archives.hpp"
#include "timemgr.hpp"

extern Page *currPage;
extern CornerButton *mainBackButton;
extern CornerButton *settingsButton;
extern Plugin *mainPlugin;
extern Plane *mainRoot;
extern linked_list list;
extern graphics::Texture *BrokenTex;

extern Allocator *fwAllocator;

userConfig conf;

int main()
{
    initMusic();
    initPaf();
    initPlugin();

    return 0;
}

void updateDarkMode()
{
    Widget::Color col;
    if(conf.darkMode)
        col = Utils::makeSceColor(.5f, .5f, .5f, .5f);
    else col = Utils::makeSceColor(1,1,1,1);  

    mainRoot->SetFilterColor(&col);
}

BUTTON_CB(DisplayInfo)
{
    new InfoPage((homeBrewInfo *)userDat, SCE_TRUE);
}

void PageListThread(void)
{
    if(list.num == 0)
    {
        SelectionList *listp = (SelectionList *)currPage;
        new TextPage("Why is there nothing here?\n>:0");
        delete listp;

        return;
    }

    node *n = list.head;
    for (int i = 0; i < list.num && n != NULL && !currPage->pageThread->EndThread; i++, n = n->next)
    {
        n->button = ((SelectionList *)currPage)->AddOption(&n->widget.title, DisplayInfo, &n->widget, SCE_TRUE, SCE_TRUE);
        sceKernelDelayThread(1000); // To make sure it doesn't crash, paf is slow sometimes
    }
    currPage->busy->Stop();

    //Texture Loading
    switch (conf.db)
    {
    case VITADB:
    case CBPSDB:
    {
        n = list.head;
        for (int i = 0; i < list.num && !currPage->pageThread->EndThread && n != NULL; i++, n = n->next)
        {
            if(checkFileExist(n->widget.icon0Local.data))
            {
                Misc::OpenResult res;
                Misc::OpenFile(&res, n->widget.icon0Local.data, SCE_O_RDONLY, 0777, NULL);

                graphics::Texture tex;

                graphics::Texture::CreateFromFile(&tex, mainPlugin->memoryPool, &res);
                n->button->SetTextureBase(&tex);


                delete res.localFile;
                sce_paf_free(res.unk_04);
            }
            else
                n->button->SetTextureBase(BrokenTex);
        }
        break;
    }

    default:
        break;
    }

    currPage->pageThread->Join();
}

void DownloadThread(void)
{
    switch (conf.db)
    {
    case VITADB:
    {
        SceInt32 res = Utils::DownloadFile(VITADB_URL, DATA_PATH "/vitadb.json");
        if(res < 0) break;

        parseJson(DATA_PATH "/vitadb.json");

        if(checkDownloadIcons() || !checkFileExist(VITADB_ICON_SAVE_PATH))
        {
            printf("Downloading Icons...\n");
            Utils::SetWidgetLabel(((LoadingPage *)currPage)->infoText, "Downloading Icons");
            Utils::DownloadFile(VITADB_DOWNLOAD_ICONS_URL, VITADB_ICON_ZIP_SAVE_PATH);
            
            LoadingPage *oldPage = (LoadingPage *)currPage;
            ProgressPage *extractPage = new ProgressPage("Extracting");
            delete oldPage;

            extractPage->busy->Stop();

            Zip *zipFile = ZipOpen(VITADB_ICON_ZIP_SAVE_PATH);
            ZipExtract(zipFile, NULL, VITADB_ICON_SAVE_PATH, extractPage->progressBars[0]);
            ZipClose(zipFile);

            saveCurrentTime();
        }
        break;
    }    
    case CBPSDB:
    {
        SceInt32 res = Utils::DownloadFile(CBPSDB_URL, DATA_PATH "/cbpsdb.csv");
        if(res < 0) break;

        parseCSV(DATA_PATH "/cbpsdb.csv");

        if(checkDownloadIcons() || !checkFileExist(CBPSDB_ICON_SAVE_PATH))
        {
            sceIoMkdir(CBPSDB_ICON_SAVE_PATH, 0777);
            LoadingPage *DownloadIndexPage = (LoadingPage *)currPage;
            ProgressPage *iconDownloadPage = new ProgressPage("Downloading Icons", 2);

            delete DownloadIndexPage;

            mainBackButton->PlayAnimationReverse(0, Widget::Animation_Reset);
            currPage->busy->Stop();

            node *n = list.head;
            for (int i = 0; i < list.num && n != NULL && !currPage->pageThread->EndThread; i++, n = n->next)
            {
                res = Utils::DownloadFile(n->widget.icon0.data, n->widget.icon0Local.data, ((ProgressPage *)currPage)->progressBars[1]);
                if(res == 6) sceIoRemove(n->widget.icon0Local.data);
                if(res != 6 && res != CURLE_OK)
                {
                    sceIoRemove(n->widget.icon0Local.data);
                    break;
                }

                //Sometimes the page is deleted and the download functiion ends this could cause a crash
                if(!currPage->pageThread->EndThread)
                {
                    double pos = i + 1;
                    double progress = (double)pos/list.num * 100.0;
                    iconDownloadPage->progressBars[0]->SetProgress(progress, 0, 0);
                }
            }
            saveCurrentTime();
            mainBackButton->PlayAnimation(0, Widget::Animation_Reset);
        }
        break;
    }
    default:
        break;
    }

    if(currPage->pageThread->EndThread == SCE_TRUE) return;

    currPage->skipAnimation = SCE_FALSE;
    delete currPage;

    SelectionList *currlist = new SelectionList();
    currlist->pageThread->Entry = PageListThread;
    currlist->pageThread->Start();
}

BUTTON_CB(DownloadIndex)
{
    new LoadingPage("Downloading Index");

    currPage->pageThread->Entry = DownloadThread;
    currPage->pageThread->Start();
}

void onReady()
{
    GetConfig(&conf);
    updateDarkMode();

    Page::Init();
    Utils::NetInit();
    Utils::MakeDataDirs();
    Utils::OverClock();

    sceSysmoduleLoadModule(SCE_SYSMODULE_JSON);

    SelectionList *categoryPage = new SelectionList("Homebrew Type");
    categoryPage->AddOption("Apps", DownloadIndex, (void *)true);
    categoryPage->busy->Stop();
}

void PrintFreeMem()
{
    /*
    printf("Free Mem: %lu\n", fwAllocator->GetFreeSize());
    */
}