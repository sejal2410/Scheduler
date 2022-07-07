#include <iostream>
#include <cstring>
#include <queue>
#include <deque>
#include <list>
#include <map>
#include <stack>

using namespace std;

enum trans_state {CREATED, TRANS_TO_READY, TRANS_TO_RUN, TRANS_TO_BLOCK, TRANS_TO_PREEMPT, TRANS_TO_TERMINATED};
//bool eventDetails = true, verbose=true;
bool eventDetails = false, verbose=false;
int CURRENT_TIME;
FILE* randomfile;
FILE* inputfile;
const char* DELIM = " \t\n\r\v\f";
vector<int> randomVals;
int randNumCount;
int maxprios=4;
bool CALL_SCHEDULER;
int ofs=-1;
std::vector<std::pair<int, int> > IOintervals;
struct Process {
    int arrival_time;
    int cpu_time;
    int CPU_burst;
    int IO_burst;
    int staticPriority;
    int dynamicPriority;
    trans_state PrevState;
    int pid;
    int state_ts;
    int cpuWaitTime;
    int remTime;
    int total_IO_time=0;
    int finishTime;
    int curr_io_burst;
    int curr_cpu_burst;
    int CPU;
};

class Event{
public:
    Event(){}
    Event(int evtTimeStamp, trans_state fromstate, trans_state toState, Process *evtProcess)  {
        this->evtTimeStamp = evtTimeStamp;
        this->fromstate =fromstate;
        this->toState = toState;
        this->evtProcess = evtProcess;
    }
    int evtTimeStamp;
    trans_state fromstate;
    trans_state toState;
    Process* evtProcess; // looking to avoid it

};

vector<Process*> processVector;
list<Event*> eventsQueue;
Process* CURRENT_RUNNING_PROCESS = NULL;

void insertEvent(Event* evt ){
    list<Event *>::iterator itr = eventsQueue.begin();
    if(eventsQueue.empty()){
        eventsQueue.push_back(evt);
        //return;
    }
    else {
        while (itr != eventsQueue.end() && (*itr)->evtTimeStamp <= evt->evtTimeStamp){
            itr++;
        }
        eventsQueue.insert(itr, evt);
    }
}

class scheduler{
public :
    string type;
    int quantum;
    bool preempt;
    virtual void add_to_run_queue(Process* proc)=0;
    virtual Process* get_next_process()=0;
    virtual bool noProcess()=0;
    virtual bool test_preempt(Process *p, int curtime )=0;
};

class FCFS : public scheduler{
    queue<Process*> run_queue;
    public :
    FCFS(){
        this->preempt = false;
        this->quantum = INT16_MAX;
        this->type = "FCFS";
    }
    void add_to_run_queue(Process* proc){
        run_queue.push(proc);
    }

    Process* get_next_process() {
        Process* next = run_queue.front();
        run_queue.pop();
        return  next;
    }
    bool noProcess() {return run_queue.empty();}
    bool test_preempt(Process *p, int curtime ){ return false;}
};

class LCFS : public scheduler{

    deque<Process*> run_queue;
    public :
    LCFS(){
        this->type = "LCFS";
        this->preempt = false;
        this->quantum = INT16_MAX;
    }
    void add_to_run_queue(Process* proc){
        run_queue.push_front(proc);
    }

    Process* get_next_process() {
        Process* next = run_queue.front();
        run_queue.pop_front();
        return  next;
    }
    bool noProcess() {return run_queue.empty();}
    bool test_preempt(Process *p, int curtime ){ return false;}
};

class SRTF : public scheduler{
    deque<Process*> run_queue;
public :
    SRTF(){
        this->type = "SRTF";
        this->preempt = false;
        this->quantum = INT16_MAX;
    }
    void add_to_run_queue(Process* proc){
        if (run_queue.empty()) {
            run_queue.push_front(proc);

        }
        else {
            deque<Process *>::iterator it = run_queue.begin();
            for (it = run_queue.begin(); it != run_queue.end(); ++it) {
                if ((*it)->cpu_time > proc->remTime)
                    break;
            }
            run_queue.insert(it, proc);
        }
    }

    Process* get_next_process() {
        Process* next = run_queue.front();
        run_queue.pop_front();
        return  next;
    }
    bool noProcess() {return run_queue.empty();}
    bool test_preempt(Process *p, int curtime ){ return false;}
};

class RR: public scheduler
{
    list <Process*> runQueue;
    public:
    RR(int quan) {
        this->type = "RR";
        this->quantum = quan;
        this->preempt = false;
    }

    void add_to_run_queue(Process *proc) {

//        if (eventDetails) {
//            cout<<"  AddProcess("<<proc->pid<<" Current:";
//            for (auto itr = runQueue.begin(); itr != runQueue.end(); itr++)
//                cout<<" "<<(*itr)->pid;
//            cout<<" ==>  ";
//        }
        runQueue.push_back(proc);
//        if (eventDetails) {
//            for (auto itr = runQueue.begin(); itr != runQueue.end(); itr++)
//                cout<<" "<<(*itr)->pid;
//            cout<<endl;
//
//        }
    }

    Process* get_next_process() {
        Process * next = runQueue.front();
        runQueue.pop_front();
        return next;
    }
    bool noProcess() {return runQueue.empty();}

    bool test_preempt(Process *p, int curtime ){ return true;} // delete fuuture events from the eventsQ
};

vector< list <Process*> > activeQueue;
vector< list <Process*> > expiredQueue;
void printQueue();
class PRIO: public scheduler
{
    int prioLevel;
public:
    PRIO(int quan, int maxPrio) {
        this->type = "PRIO";
        this->quantum = quan;
        this->preempt = false;
        this->prioLevel = maxPrio;
        for(int i=0;i<maxPrio;i++) {
            list<Process*> activelist;
            list<Process*> expiredlist;
            activeQueue.push_back(activelist);
            expiredQueue.push_back(expiredlist);
        }

    }

    void add_to_run_queue(Process *proc) {
       // printQueue();
        if(proc->dynamicPriority>=0){
            int level = proc->dynamicPriority;
            activeQueue[level].push_back(proc);
        }
        else{
            int level = proc->staticPriority-1;
            proc->dynamicPriority=level;
            expiredQueue[level].push_back(proc);
        }
    }

    Process* get_next_process() {
        //Process * next = activeQueue.front();
        for(int i =this->prioLevel-1;i>=0;i--){
            if(!activeQueue[i].empty()) {
               Process* next =  activeQueue[i].front();
                activeQueue[i].pop_front();
                return next;
            }
        }
        //swap the queue;
        activeQueue.swap(expiredQueue);
        Process* next;
        for(int i =this->prioLevel-1;i>=0;i--){
            if(!activeQueue[i].empty()) {
                   next =  activeQueue[i].front();
                    activeQueue[i].pop_front();
                    break;
                }
        }
        return next;

    }
    bool noProcess() {
        for(int i =0;i<this->prioLevel;i++){
            if(!activeQueue[i].empty())
                return false;
        }
        for(int i =0;i<this->prioLevel;i++){
            if(!expiredQueue[i].empty())
                return false;
        }
        return true;
    }

    bool test_preempt(Process *p, int curtime ){ return true;} // delete fuuture events from the eventsQ
};

class EPRIO: public scheduler
{
    int prioLevel;
public:
    EPRIO(int quan, int maxPrio) {
        this->type = "PREPRIO";
        this->quantum = quan;
        this->preempt = true;
        this->prioLevel = maxPrio;
        for(int i=0;i<maxPrio;i++) {
            list<Process*> activelist;
            list<Process*> expiredlist;
            activeQueue.push_back(activelist);
            expiredQueue.push_back(expiredlist);
        }

    }

    void add_to_run_queue(Process *proc) {
        // printQueue();
        if(proc->dynamicPriority>=0){
            int level = proc->dynamicPriority;
            activeQueue[level].push_back(proc);
        }
        else{
            int level = proc->staticPriority-1;
            proc->dynamicPriority=level;
            expiredQueue[level].push_back(proc);
        }
    }

    Process* get_next_process() {
        //Process * next = activeQueue.front();
        for(int i =this->prioLevel-1;i>=0;i--){
            if(!activeQueue[i].empty()) {
                Process* next =  activeQueue[i].front();
                activeQueue[i].pop_front();
                return next;
            }
        }
        //swap the queue;
        activeQueue.swap(expiredQueue);
        Process* next;
        for(int i =this->prioLevel-1;i>=0;i--){
            if(!activeQueue[i].empty()) {
                next =  activeQueue[i].front();
                activeQueue[i].pop_front();
                break;
            }
        }
        return next;

    }
    bool noProcess() {
        for(int i =0;i<this->prioLevel;i++){
            if(!activeQueue[i].empty())
                return false;
        }
        for(int i =0;i<this->prioLevel;i++){
            if(!expiredQueue[i].empty())
                return false;
        }
        return true;
    }
//    bool futureEvents(Event *evt){
//        if(evt->evtProcess->pid==CURRENT_RUNNING_PROCESS->pid && evt->evtTimeStamp> CURRENT_TIME)
//            return true;
//        return false;
//    }
    bool test_preempt(Process *p, int curtime ){
        //remove events for current running process
        //eventsQueue.remove_if(futureEvents);
        for (auto it=eventsQueue.begin();it!=eventsQueue.end();it++){
            if((*it)->evtProcess->pid==CURRENT_RUNNING_PROCESS->pid) {
                eventsQueue.erase(it);
                return true;
            }
        }
        return true;
    }
};

void readRandomFile(){
    char line[1024];
    fgets(line,1024, randomfile);
    char* tok = strtok(line, DELIM);
    randNumCount = atoi(tok);
    while(fgets(line,1024, randomfile)){
        char* tok = strtok(line, DELIM);
        randomVals.push_back(atoi(tok));
    }
}

int myrandom(int burst) {
//    if(ofs==8)
//        cout<< "I/O burst at this instant is: "<<burst;
    ofs++;

    if(ofs==randNumCount)
        ofs=0;

  //  cout<<"Current time: "<<CURRENT_TIME<<" Random Offset: "<<ofs<<endl;
    return 1 + (randomVals[ofs] % burst);
}

scheduler *SCHEDULER;
char* state_map(trans_state state){
    switch (state) {
        case CREATED:
            return "CREATED";
        case TRANS_TO_RUN:
            return "RUNNG";
        case TRANS_TO_BLOCK:
            return "BLOCK";
        case TRANS_TO_READY:
            return "READY";
        case TRANS_TO_PREEMPT:
            return "PREMPT";
        case TRANS_TO_TERMINATED:
            return "DONE";

    }
}
void readInputFile() {
        char line[1024];
        int i = -1;
        while(fgets(line,1024, inputfile)){
            i++;
            Process* p = new Process();
            char* tok = strtok(line, DELIM);
            p->arrival_time = atoi(tok);
            char* ct = strtok(NULL, DELIM);
            p->cpu_time = atoi(ct);
            char* cb = strtok(NULL, DELIM);
            p->CPU_burst = atoi(cb);
            char* ib = strtok(NULL, DELIM);
            p->IO_burst = atoi(ib);
            p->pid = i;
            p->state_ts = p->arrival_time;
            p->curr_cpu_burst = 0;
            p->remTime = p->cpu_time;
            p->staticPriority = myrandom(maxprios);
            p->dynamicPriority = p->staticPriority-1;

            Event* evt = new Event();
            evt->evtTimeStamp = p->arrival_time;
            evt->fromstate = CREATED;
            evt->toState = TRANS_TO_READY;
            evt->evtProcess = p;
            eventsQueue.push_back(evt);
            processVector.push_back(p);
        }

}
void printQueue(){
        cout<<"{ ";
        for(int i=0;i<activeQueue.size();i++){
            cout<<"[";
            for (auto v : activeQueue[i])
                cout<<v->pid<<",";
            cout<<"] ";
        }
        cout<<"} : { ";
        for(int i=0;i<expiredQueue.size();i++){
            cout<<"[";
            for (auto v : expiredQueue[i])
                cout<<v->pid<<",";
            cout<<"] ";
        }
        cout<<"}\n";

}
void add_event(Event* e){

    if (eventDetails) {
        //printQueue();
        cout<<"  AddEvent("<<e->evtTimeStamp<<":"<<e->evtProcess->pid<<":"<<state_map(e->toState)<<"):";
        for (auto itr = eventsQueue.begin(); itr != eventsQueue.end(); itr++)
            cout<<" "<<(*itr)->evtTimeStamp<<":"<<(*itr)->evtProcess->pid<<":"<<state_map((*itr)->toState);
        cout<<" ==>  ";

    }
    insertEvent(e);
    if (eventDetails) {
        for (auto itr = eventsQueue.begin(); itr != eventsQueue.end(); itr++)
            cout<<" "<<(*itr)->evtTimeStamp<<":"<<(*itr)->evtProcess->pid<<":"<<state_map((*itr)->toState);
        cout<<endl;
    }

}
void Simulation() {
    Event* evt;

    while(!eventsQueue.empty()) {
        evt = eventsQueue.front();

        if(evt== nullptr){
            eventsQueue.pop_front();
            continue;
        }
        Process *proc = evt->evtProcess;
        eventsQueue.pop_front();// this is the process the event works on
        CURRENT_TIME = evt->evtTimeStamp;
        trans_state transition = evt->toState;
        trans_state fromstate = evt->fromstate;
        int timeInPrevState = CURRENT_TIME - proc->state_ts;
        delete evt;
        evt = nullptr; // remove cur event obj and don’t touch anymore
        if(verbose)
            cout << CURRENT_TIME << " " <<proc->pid << " " << timeInPrevState << ": "
            << state_map(fromstate)<<" -> " << state_map(transition);

        switch(transition) { // which state to transition to?
            case TRANS_TO_READY:
// must come from BLOCKED or from PREEMPTION
// must add to run queue
            {

                if(verbose)
                    cout << " cb=" << proc->curr_cpu_burst << " rem=" << proc->remTime << " prio=" << proc->dynamicPriority<<endl;
                  //  cout<<endl;


                if(fromstate==TRANS_TO_BLOCK){
                    proc->total_IO_time+=timeInPrevState;
                    proc->dynamicPriority = proc->staticPriority-1;
//                    if(CURRENT_TIME>=60)
//                        cout<<"Dyanamic Prio: "<<proc->dynamicPriority<<" PID: "<<proc->pid<<endl;
                  }
                else if(fromstate==TRANS_TO_RUN) {
                    proc->dynamicPriority--;
                    CURRENT_RUNNING_PROCESS= nullptr;
                }
//                if(CURRENT_TIME>=40 && CURRENT_RUNNING_PROCESS!=NULL)
//                    cout<<CURRENT_RUNNING_PROCESS->pid<<"   proc DynamicPrio: "<<proc->dynamicPriority<<"   Current Dynamic Prio: "<<CURRENT_RUNNING_PROCESS->dynamicPriority<<endl;
                if(SCHEDULER->preempt && CURRENT_RUNNING_PROCESS!=NULL && CURRENT_RUNNING_PROCESS->dynamicPriority<proc->dynamicPriority&&
                        !(CURRENT_RUNNING_PROCESS->state_ts+CURRENT_RUNNING_PROCESS->CPU==CURRENT_TIME)){
                    //  CURRENT_RUNNING_PROCESS->dynamicPriority--;
//                 if(CURRENT_TIME>=40)
//                       cout<<"HI"<<endl;
                    int cpu_done = CURRENT_TIME-CURRENT_RUNNING_PROCESS->state_ts;
//                    if(CURRENT_TIME>=40)
//                      cout<< CURRENT_TIME<<"  "<<CURRENT_RUNNING_PROCESS->state_ts<<endl;
                    //if(()

                    CURRENT_RUNNING_PROCESS->remTime+=(CURRENT_RUNNING_PROCESS->CPU-cpu_done);
                    CURRENT_RUNNING_PROCESS->curr_cpu_burst+=(CURRENT_RUNNING_PROCESS->CPU-cpu_done);
                    //CURRENT_RUNNING_PROCESS->state_ts = CURRENT_TIME;
                    SCHEDULER->test_preempt(CURRENT_RUNNING_PROCESS,CURRENT_TIME);
                    add_event(new Event(CURRENT_TIME,TRANS_TO_RUN,TRANS_TO_READY,CURRENT_RUNNING_PROCESS));
                    CURRENT_RUNNING_PROCESS =NULL;
                }
                proc->state_ts = CURRENT_TIME;
                SCHEDULER->add_to_run_queue(proc);
                CALL_SCHEDULER = true; // conditional on whether something is run
                break;
            }
            case TRANS_TO_RUN:
// create event for either preemption or blocking
            {
                    int cpuBurst =0;
                    if(proc->curr_cpu_burst!=0) {
                        cpuBurst = proc->curr_cpu_burst;
                        proc->curr_cpu_burst = 0;
                    }
                    else{
                        int randomBurst = myrandom(proc->CPU_burst);
                       // cout<<"CPU randomBurst: "<<randomBurst<<endl;
                        cpuBurst = min(proc->remTime,randomBurst );
                        proc->curr_cpu_burst = 0;
                    }
                   // proc->remTime-=proc->curr_cpu_burst;
                    //check if rem+cpuBurst = totalcpu time and add to done state
                if(verbose)
                    cout << " cb=" << cpuBurst << " rem=" << proc->remTime << " prio=" << proc->dynamicPriority<<endl;
                if(cpuBurst>SCHEDULER->quantum){
                    proc->remTime -=SCHEDULER->quantum;
                    proc->curr_cpu_burst=cpuBurst-SCHEDULER->quantum;
                    cpuBurst = SCHEDULER->quantum;
                    add_event(new Event(CURRENT_TIME+SCHEDULER->quantum,TRANS_TO_RUN, TRANS_TO_READY, proc));
                }
                else if(proc->remTime-cpuBurst<=0) {
                        proc->remTime = 0;
                        proc->curr_cpu_burst = 0;
                        add_event(new Event(CURRENT_TIME+cpuBurst,TRANS_TO_RUN, TRANS_TO_TERMINATED, proc));
                    }
                    //prempt condition check

                    // Otherwise Event for I/O is generated that is block stage
                    else{
                        proc->remTime -= cpuBurst;
                        proc->curr_cpu_burst = 0;
                     //   cout<<"Entered I/O here CPU burst is: "<<cpuBurst<<endl;
                        add_event(new Event(CURRENT_TIME+cpuBurst,TRANS_TO_RUN, TRANS_TO_BLOCK, proc));
                    }
                    proc->CPU=cpuBurst;
                    proc->state_ts = CURRENT_TIME;
                    proc->cpuWaitTime +=timeInPrevState;
                break;
            }
            case TRANS_TO_BLOCK:
//create an event for when process becomes READY again
            {
//                if(!(fromstate==TRANS_TO_RUN )){
//                    cout<<"Error in transition";
//                    abort();
//                }
                int ioburst = myrandom(proc->IO_burst);
                proc->curr_io_burst=ioburst;
                if(verbose)
                    cout << " ib=" << proc->curr_io_burst <<" rem=" << proc->remTime<<endl;
                add_event(new Event(CURRENT_TIME+ioburst,TRANS_TO_BLOCK, TRANS_TO_READY,proc));
                IOintervals.push_back(std::make_pair(CURRENT_TIME, CURRENT_TIME+ioburst));
                proc->state_ts = CURRENT_TIME;
                CALL_SCHEDULER = true;
                CURRENT_RUNNING_PROCESS = nullptr;
                break;
            }
//            case TRANS_TO_PREEMPT:
//// add to runqueue (no event is generated)
//            {
//                if(verbose)
//                    cout<<endl;
//                SCHEDULER->add_to_run_queue(proc);
//                CALL_SCHEDULER = true;
//                break;
//            }

            case TRANS_TO_TERMINATED :
            {
                proc->finishTime= CURRENT_TIME;
                CURRENT_RUNNING_PROCESS = nullptr;
                CALL_SCHEDULER = true;
                if (verbose)
                    cout << endl;
                break;
            }
            default :
                printf("Invalid State");
        }
       // cout<<"Dyanamic Prio: "<<proc->dynamicPriority<<" PID: "<<proc->pid<<endl;

        delete evt;
        if (CALL_SCHEDULER) {
           Event* nextEvent= eventsQueue.front();
           if (nextEvent!=NULL && nextEvent->evtTimeStamp == CURRENT_TIME) continue;
           // printQueue();
            CALL_SCHEDULER = false; // reset global flag
            if (CURRENT_RUNNING_PROCESS == NULL && !SCHEDULER->noProcess()) {
                CURRENT_RUNNING_PROCESS = SCHEDULER->get_next_process();
                if (CURRENT_RUNNING_PROCESS == NULL)
                    continue;
              //  cout<<" New Current Running Process  pid: "<< CURRENT_RUNNING_PROCESS->pid;
                Event *  newEvt = new Event(CURRENT_TIME, TRANS_TO_READY, TRANS_TO_RUN, CURRENT_RUNNING_PROCESS);
                add_event(newEvt);
            }
            else if(CURRENT_RUNNING_PROCESS!=NULL) {
              //  cout << "CURRENT RUNNING PROCESS is not null pid: " << CURRENT_RUNNING_PROCESS->pid << endl;
            }
        }

    }
}

stack<pair<int, int> > mergeIntervals( std::vector<std::pair<int, int> > arr)
{
    stack<pair<int, int> > s;
    if (arr.empty())
        return s;
    sort(arr.begin(), arr.end());
    s.push(arr[0]);

    for (int i = 1 ; i < arr.size(); i++)
    {
        // get interval from stack top
        pair<int, int> top = s.top();

        // if current interval is not overlapping with stack top,
        // push it to the stack
        if (top.second < arr[i].first)
            s.push(arr[i]);

            // Otherwise update the ending time of top if ending of current
            // interval is more
        else if (top.second < arr[i].second)
        {
            top.second = arr[i].second;
            s.pop();
            s.push(top);
        }
    }
    return s;
}

int parseQuantum(string s){
    return  stoi(s);
}


int main(int argc, char *argv[]){
    char schedulerType;
    int schedstart = 0;
    while(!(std::string(argv[schedstart++]).rfind("-s", 0) == 0));
    if(schedstart==argc) {
        cout << "Invalid Argument";
        exit(0);
    }
    else {
        string scheduler = argv[--schedstart];
        schedulerType = scheduler.at(2);
        if (schedulerType == 'F') {
            SCHEDULER = new FCFS();
        } else if (schedulerType == 'L') {
            SCHEDULER = new LCFS();
        }
        else if (schedulerType == 'S') {
            SCHEDULER = new SRTF();
        }
        else if (schedulerType == 'R') {
            int quantnum = parseQuantum(scheduler.substr (3));
            SCHEDULER = new RR(quantnum);
        }
        else if(schedulerType=='P'){
            int posColon = scheduler.find(":");
            int quantnum;
            if(posColon==string::npos)
                quantnum= parseQuantum(scheduler.substr (3));
            else{
//                cout<<scheduler.substr (3, posColon-4);
                quantnum = parseQuantum(scheduler.substr (3, posColon-3));
                maxprios = parseQuantum(scheduler.substr(posColon+1));
            }
            //cout<<quantnum<<" maxprios:"<<maxprios;
            SCHEDULER = new PRIO(quantnum, maxprios);
        }
        else if(schedulerType=='E'){

            int quantnum = parseQuantum(scheduler.substr (3));

            SCHEDULER = new EPRIO(quantnum, maxprios);
        }
    }
    randomfile = fopen(argv[schedstart+2], "r");
    inputfile = fopen(argv[schedstart+1], "r");
    readRandomFile();
    readInputFile();
    Simulation();
    //OutputIOintervals = {std::vector<std::pair<int, int>>} size=10
    vector<Process *>::iterator itr = processVector.begin();

    stack<pair<int, int> > s = mergeIntervals(IOintervals);
    //stack<pair<int, int> >::iterator it = s.();
    if(SCHEDULER->type=="RR" || SCHEDULER->type=="PRIO" ||SCHEDULER->type=="PREPRIO")
    printf("%s %d\n", SCHEDULER->type.c_str(), SCHEDULER->quantum);
    else
        printf("%s\n", SCHEDULER->type.c_str());
    double totalCPUTime=0, totalIOTime=0, totalTAT=0, totalWT=0, noProcesses=0;

    while(!s.empty()){
        pair<int, int > p = s.top();
        s.pop();
        totalIOTime+= (p.second-p.first);
    }


    while (itr != processVector.end()) {
        noProcesses++;
        printf("%04d: %4d %4d %4d %4d %1d | %5d %5d %5d %5d\n",(*itr)->pid,(*itr)->arrival_time,
               (*itr)->cpu_time,(*itr)->CPU_burst,(*itr)->IO_burst,(*itr)->staticPriority,(*itr)->finishTime,
               ((*itr)->finishTime - (*itr)->arrival_time),(*itr)->total_IO_time,(*itr)->cpuWaitTime);
        totalCPUTime+=(*itr)->cpu_time;
      //  totalIOTime+=(*itr)->total_IO_time;
        totalTAT+=((*itr)->finishTime-(*itr)->arrival_time);
        totalWT+=(*itr)->cpuWaitTime;
        itr++;
    }
    double cpuUtilization = (totalCPUTime)/CURRENT_TIME*100;
    double avgTAT = (totalTAT)/noProcesses;
    double avgWT = (totalWT)/noProcesses;
    double IOUtilization = (totalIOTime)/CURRENT_TIME*100;
    double throughput = (noProcesses/CURRENT_TIME)*100;
    printf("SUM: %d %.2lf %.2lf %.2lf %.2lf %.3lf\n",CURRENT_TIME,cpuUtilization,IOUtilization, avgTAT,
           avgWT,throughput);
    return 0;
}

