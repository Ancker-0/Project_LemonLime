/***************************************************************************
    This file is part of Project Lemon
    Copyright (C) 2011 Zhipeng Jia

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
***************************************************************************/
/**
 * assignmentthread.cpp @Project Lemon+
 * Update 2018 Dust1404
 **/
/**
 * assignmentthread.cpp @Project LemonLime
 * Update 2019 iotang
 **/

#include "globaltype.h"
#include "assignmentthread.h"
#include "judgingthread.h"
#include "settings.h"
#include "compiler.h"
#include "task.h"
#include "testcase.h"
#include "judgesharedvariables.h"

AssignmentThread::AssignmentThread(QObject *parent) :
	QThread(parent)
{
	moveToThread(this);
	curTestCaseIndex = 0;
	curSingleCaseIndex = 0;
	countFinished = 0;
	totalSingleCase = 0;
	stopJudging = false;
}

void AssignmentThread::setSettings(Settings *_settings)
{
	settings = _settings;
}

void AssignmentThread::setTask(Task *_task)
{
	task = _task;
}

void AssignmentThread::setContestantName(const QString &name)
{
	contestantName = name;
}

CompileState AssignmentThread::getCompileState() const
{
	return compileState;
}

const QString &AssignmentThread::getCompileMessage() const
{
	return compileMessage;
}

const QString &AssignmentThread::getSourceFile() const
{
	return sourceFile;
}

const QList< QList<int> > &AssignmentThread::getScore() const
{
	return score;
}

const QList< QList<int> > &AssignmentThread::getTimeUsed() const
{
	return timeUsed;
}

const QList< QList<int> > &AssignmentThread::getMemoryUsed() const
{
	return memoryUsed;
}

const QList< QList<ResultState> > &AssignmentThread::getResult() const
{
	return result;
}

const QList<QStringList> &AssignmentThread::getMessage() const
{
	return message;
}

const QList<QStringList> &AssignmentThread::getInputFiles() const
{
	return inputFiles;
}

bool AssignmentThread::traditionalTaskPrepare()
{
	compileState = NoValidSourceFile;
	QDir contestantDir;
	contestantDir = ! task->getSubFolderCheck() ? QDir(Settings::sourcePath() + contestantName) : QDir(Settings::sourcePath() + contestantName + QDir::separator() + task->getSourceFileName());

	QList<Compiler *> compilerList = settings->getCompilerList();

	for (int i = 0; i < compilerList.size(); i ++)
	{
		if (task->getCompilerConfiguration(compilerList[i]->getCompilerName()) == "disable") continue;

		QStringList filters;

		if (task->getTaskType() == Task::Communication)
		{
			filters = task->getSourceFilesName();
		}
		else
		{
			filters = compilerList[i]->getSourceExtensions();
			for (int j = 0; j < filters.size(); j ++)
			{
				filters[j] = task->getSourceFileName() + "." + filters[j];
			}
		}

		QStringList files = contestantDir.entryList(filters, QDir::Files);
		sourceFile = "";

		for (int j = 0; j < files.size(); j ++)
		{
			qint64 fileSize = QFileInfo(Settings::sourcePath() + contestantName + (task->getSubFolderCheck() ? QDir::separator() + task->getSourceFileName() : QString("")) + QDir::separator() + files[j]).size();

			if (fileSize <= settings->getFileSizeLimit() * 1024)
			{
				if (task->getTaskType() == Task::Communication)
				{
					sourceFile = sourceFile + " " + files[j] + " ";
				}
				else
				{
					sourceFile = files[j];
					break;
				}
			}
		}

		if (! sourceFile.isEmpty())
		{
			QDir(Settings::temporaryPath()).mkdir(contestantName);
			if (task->getTaskType() == Task::Communication)
			{
				QStringList sourcePaths = task->getSourceFilesPath(), sourceFiles = task->getSourceFilesName();
				for (int i = 0; i < sourceFiles.length(); i++)
				{
					QFile::copy(Settings::sourcePath() + contestantName + (task->getSubFolderCheck() ? QDir::separator() + task->getSourceFileName() : QString("")) + QDir::separator() + sourcePaths[i],
					            Settings::temporaryPath() + contestantName + QDir::separator() + sourceFiles[i]);
				}
			}
			else
			{
				QFile::copy(Settings::sourcePath() + contestantName + (task->getSubFolderCheck() ? QDir::separator() + task->getSourceFileName() : QString("")) + QDir::separator() + sourceFile,
				            Settings::temporaryPath() + contestantName + QDir::separator() + sourceFile);
			}

			QString extraFiles = "";

			if (task->getTaskType() == Task::Interaction)
			{
				QFile::copy(Settings::dataPath() + task->getInteractor(), Settings::temporaryPath() + contestantName + QDir::separator() + task->getInteractorName());
				QFile::copy(Settings::dataPath() + task->getGrader(), Settings::temporaryPath() + contestantName + QDir::separator() + "__grader.cpp");
			}

			if (task->getTaskType() == Task::Communication)
			{
				QStringList graderPath = task->getGraderFilesPath(), graderName = task->getGraderFilesName();
				for (int i = 0; i < graderPath.length(); i++)
				{
					QFile::copy(Settings::dataPath() + graderPath[i], Settings::temporaryPath() + contestantName + QDir::separator() + graderName[i]);
					extraFiles = extraFiles + " " + graderName[i] + " ";
				}
			}

			QStringList configurationNames = compilerList[i]->getConfigurationNames();
			QStringList compilerArguments = compilerList[i]->getCompilerArguments();
			QStringList interpreterArguments = compilerList[i]->getInterpreterArguments();
			QString currentConfiguration = task->getCompilerConfiguration(compilerList[i]->getCompilerName());

			for (int j = 0; j < configurationNames.size(); j ++)
			{
				if (configurationNames[j] == currentConfiguration)
				{
					timeLimitRatio = compilerList[i]->getTimeLimitRatio();
					memoryLimitRatio = compilerList[i]->getMemoryLimitRatio();
					disableMemoryLimitCheck = compilerList[i]->getDisableMemoryLimitCheck();
					environment = compilerList[i]->getEnvironment();
					QStringList values = QProcessEnvironment::systemEnvironment().toStringList();

					for (int k = 0; k < values.size(); k ++)
					{
						int tmp = values[k].indexOf("=");

						if (tmp == 0) continue;

						QString variable = values[k].mid(0, tmp);

						if (environment.contains(variable))
							environment.insert(variable,
							                   environment.value(variable) + ";"
							                   + QProcessEnvironment::systemEnvironment().value(variable));
						else
							environment.insert(variable, QProcessEnvironment::systemEnvironment().value(variable));
					}

					if (compilerList[i]->getCompilerType() == Compiler::Typical)
					{
#ifdef Q_OS_WIN32
						executableFile = task->getSourceFileName() + ".exe";
#else
						executableFile = task->getSourceFileName();
#endif
						interpreterFlag = false;
					}
					else
					{
						executableFile = compilerList[i]->getInterpreterLocation();
						arguments = interpreterArguments[j];
						arguments.replace("%s.*", sourceFile + extraFiles);
						arguments.replace("%s", task->getSourceFileName());
						interpreterFlag = true;
					}

					if (compilerList[i]->getCompilerType() != Compiler::InterpretiveWithoutByteCode)
					{
						QString arguments = compilerArguments[j];

						if (task->getTaskType() == Task::Interaction)
						{
							arguments.replace("%s.*", sourceFile + " __grader.cpp");
						}
						else if (task->getTaskType() == Task::Communication)
						{
							arguments.replace("%s.*", sourceFile + extraFiles);
						}
						else arguments.replace("%s.*", sourceFile);

						arguments.replace("%s", task->getSourceFileName());
						QProcess *compiler = new QProcess(this);
						compiler->setProcessChannelMode(QProcess::MergedChannels);
						compiler->setProcessEnvironment(environment);
						compiler->setWorkingDirectory(Settings::temporaryPath() + contestantName);
						compiler->start(QString("\"") + compilerList[i]->getCompilerLocation() + "\" " + arguments);

						if (! compiler->waitForStarted(-1))
						{
							compileState = InvalidCompiler;
							delete compiler;
							break;
						}

						QElapsedTimer timer;
						timer.start();
						bool flag = false;

						while (timer.elapsed() < settings->getCompileTimeLimit())
						{
							if (compiler->state() != QProcess::Running)
							{
								flag = true;
								break;
							}

							QCoreApplication::processEvents();

							if (stopJudging)
							{
								compiler->kill();
								delete compiler;
								return false;
							}

							msleep(10);
						}

						if (! flag)
						{
							compiler->kill();
							compileState = CompileTimeLimitExceeded;
						}
						else if (compiler->exitCode() != 0)
						{
							compileState = CompileError;
							compileMessage = QString::fromLocal8Bit(compiler->readAllStandardOutput().data());
						}
						else
						{
							if (compilerList[i]->getCompilerType() == Compiler::Typical)
							{
								if (! QDir(Settings::temporaryPath() + contestantName).exists(executableFile))
								{
									compileState = InvalidCompiler;
								}
								else
								{
									compileState = CompileSuccessfully;
								}
							}
							else
							{
								QStringList filters = compilerList[i]->getBytecodeExtensions();

								for (int k = 0; k < filters.size(); k ++)
								{
									filters[k] = QString("*.") + filters[k];
								}

								if (QDir(Settings::temporaryPath() + contestantName)
								      .entryList(filters, QDir::Files).size() == 0)
								{
									compileState = InvalidCompiler;
								}
								else
								{
									compileState = CompileSuccessfully;
								}
							}
						}

						delete compiler;
					}

					if (compilerList[i]->getCompilerType() == Compiler::InterpretiveWithoutByteCode)
						compileState = CompileSuccessfully;

					break;
				}
			}

			break;
		}
	}

	if (compileState != CompileSuccessfully)
	{
		emit compileError(task->getTotalTimeLimit(), static_cast<int>(compileState));
		return false;
	}

	return true;
}

void AssignmentThread::run()
{
	if (task->getTaskType() != Task::AnswersOnly)
		if (! traditionalTaskPrepare()) return;

	if (stopJudging) return;

	for (int i = 0; i < task->getTestCaseList().size(); i ++)
	{
		timeUsed.append(QList<int>());
		memoryUsed.append(QList<int>());
		score.append(QList<int>());
		result.append(QList<ResultState>());
		overallStatus.append(2);
		message.append(QStringList());
		inputFiles.append(QStringList());
		testCaseScore.append(task->getTestCase(i)->getFullScore());

		for (int j = 0; j < task->getTestCase(i)->getInputFiles().size(); j ++)
		{
			timeUsed[i].append(-1);
			memoryUsed[i].append(-1);
			score[i].append(0);
			result[i].append(Skipped);
			message[i].append("");
			inputFiles[i].append("");
		}
	}


	skipEnabled = 0;

	assign();

	exec();
}

void AssignmentThread::assign()
{
	if (curTestCaseIndex == task->getTestCaseList().size())
	{
		if (countFinished == totalSingleCase) quit();

		return;
	}

	TestCase *curTestCase = task->getTestCase(curTestCaseIndex);

	bool beingSkipped = 0;

	if (curSingleCaseIndex == curTestCase->getInputFiles().size())
	{
		curTestCaseIndex ++;

		while (curTestCaseIndex < task->getTestCaseList().size())
		{
			if (task->getTestCase(curTestCaseIndex)->getInputFiles().size() > 0) break;

			curTestCaseIndex ++;
		}

		curSingleCaseIndex = 0;

		if (curTestCaseIndex == task->getTestCaseList().size())
		{
			if (countFinished == totalSingleCase) quit();

			return;
		}

		curTestCase = task->getTestCase(curTestCaseIndex);
		const QList<int> &dependenceSubtask(curTestCase->getDependenceSubtask());

		for (int i = 0; i != dependenceSubtask.size(); ++i)
		{
			int status = overallStatus[dependenceSubtask[i] - 1];
			emit singleSubtaskDependenceFinished(curTestCaseIndex, i, status);

			if (status <= 0)beingSkipped = 1;
		}

		if (! dependenceSubtask.empty())
			score[curTestCaseIndex].push_back(-1 - beingSkipped);
	}

	totalSingleCase ++;
	curTestCase = task->getTestCase(curTestCaseIndex);
	inputFiles[curTestCaseIndex][curSingleCaseIndex]
	   = QFileInfo(curTestCase->getInputFiles().at(curSingleCaseIndex)).fileName();

	if (overallStatus[curTestCaseIndex] <= 0 || beingSkipped)
	{
		overallStatus[curTestCaseIndex] = 0;
		taskSkipped(qMakePair(curTestCaseIndex, curSingleCaseIndex++));
		return;
	}

	JudgingThread *thread = new JudgingThread();
	thread->setExtraTimeRatio(0.1);
	QString workingDirectory = QDir::toNativeSeparators(QDir(Settings::temporaryPath()
	                           + QString("_%1.%2").arg(curTestCaseIndex).arg(curSingleCaseIndex))
	                           .absolutePath()) + QDir::separator();
	thread->setWorkingDirectory(workingDirectory);
	QDir(Settings::temporaryPath()).mkdir(QString("_%1.%2").arg(curTestCaseIndex).arg(curSingleCaseIndex));
	QStringList entryList = QDir(Settings::temporaryPath() + contestantName).entryList(QDir::Files);

	for (int i = 0; i < entryList.size(); i ++)
	{
		QFile::copy(Settings::temporaryPath() + contestantName + QDir::separator() + entryList[i],
		            workingDirectory + entryList[i]);
	}

	thread->setSpecialJudgeTimeLimit(settings->getSpecialJudgeTimeLimit());
	thread->setDiffPath(settings->getDiffPath());

	if (task->getTaskType() == Task::Traditional || task->getTaskType() == Task::Interaction || task->getTaskType() == Task::Communication)
	{
		if (interpreterFlag)
		{
			thread->setExecutableFile(executableFile);
		}
		else
		{
			thread->setExecutableFile(workingDirectory + executableFile);
		}

		thread->setArguments(arguments);
	}

	if (task->getTaskType() == Task::AnswersOnly)
	{
		QString fileName;
		fileName = QFileInfo(curTestCase->getInputFiles().at(curSingleCaseIndex)).completeBaseName();
		fileName += QString(".") + task->getAnswerFileExtension();

		if (! task->getSubFolderCheck())
			thread->setAnswerFile(Settings::sourcePath() + contestantName + QDir::separator() + fileName);
		else
			thread->setAnswerFile(Settings::sourcePath() + contestantName + QDir::separator() + task->getInputFileName() + QDir::separator() + fileName);
	}

	thread->setTask(task);

	connect(thread, SIGNAL(finished()), this, SLOT(threadFinished()));
	connect(this, SIGNAL(stopJudgingSignal()), thread, SLOT(stopJudgingSlot()));

	thread->setInputFile(Settings::dataPath() + curTestCase->getInputFiles().at(curSingleCaseIndex));
	thread->setOutputFile(Settings::dataPath() + curTestCase->getOutputFiles().at(curSingleCaseIndex));
	thread->setFullScore(curTestCase->getFullScore());

	if (task->getTaskType() != Task::AnswersOnly)
	{
		thread->setEnvironment(environment);
		thread->setTimeLimit(qCeil(curTestCase->getTimeLimit() * timeLimitRatio));

		if (disableMemoryLimitCheck)
		{
			thread->setMemoryLimit(-1);
		}
		else
		{
			thread->setMemoryLimit(qCeil(curTestCase->getMemoryLimit() * memoryLimitRatio));
		}
	}

	running[thread] = qMakePair(curTestCaseIndex, curSingleCaseIndex ++);
	thread->start();
}

void AssignmentThread::taskSkipped(const QPair<int, int> &cur)
{
	++countFinished;
	emit singleCaseFinished(task->getTestCase(cur.first)->getTimeLimit(),
	                        cur.first,
	                        cur.second,
	                        int (result[cur.first][cur.second]),
	                        0, 0, 0);
	assign();
}

void AssignmentThread::threadFinished()
{
	JudgingThread *thread = dynamic_cast<JudgingThread *>(sender());

	if (stopJudging)
	{
		running.remove(thread);
		delete thread;

		if (running.size() == 0) quit();

		return;
	}

	if (thread->getNeedRejudge() && thread->getJudgeTimes() != settings->getRejudgeTimes() + 1)
	{
		thread->start();
	}
	else
	{
		QPair<int, int> cur = running[thread];
		timeUsed[cur.first][cur.second] = thread->getTimeUsed();
		memoryUsed[cur.first][cur.second] = thread->getMemoryUsed();
		score[cur.first][cur.second] = thread->getScore();
		result[cur.first][cur.second] = thread->getResult();
		overallStatus[cur.first] = qMin(overallStatus[cur.first], stateToStatus(thread->getResult()));
		message[cur.first][cur.second] = thread->getMessage();
		running.remove(thread);
		countFinished ++;
		delete thread;

		int nowScore = score[cur.first][cur.second];
		if (cur.second + 1 == task->getTestCase(cur.first)->getInputFiles().size())
		{
			for (int i = 0; i < cur.second; i++)
				nowScore = qMin(nowScore, score[cur.first][i]);
		}

		emit singleCaseFinished(task->getTestCase(cur.first)->getTimeLimit(),
		                        cur.first,
		                        cur.second,
		                        int (result[cur.first][cur.second]),
		                        (cur.second + 1 == task->getTestCase(cur.first)->getInputFiles().size() ? 1 : -1) * nowScore,
		                        timeUsed[cur.first][cur.second],
		                        memoryUsed[cur.first][cur.second]
		                       );

		if (score[cur.first][cur.second] < testCaseScore[cur.first])
			testCaseScore[cur.first] = score[cur.first][cur.second];

		assign();
	}
}

void AssignmentThread::stopJudgingSlot()
{
	stopJudging = true;
	emit stopJudgingSignal();
}
