/**
 *
 *
 * Copyright (c) 2010 / 2011 eZuce, Inc. All rights reserved.
 * Contributed to SIPfoundry under a Contributor Agreement
 *
 * This software is free software; you can redistribute it and/or modify it under
 * the terms of the Affero General Public License (AGPL) as published by the
 * Free Software Foundation; either version 3 of the License, or (at your option)
 * any later version.
 *
 * This software is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more
 * details.
 */
package org.sipfoundry.sipxconfig.openacd;

import java.util.ArrayList;
import java.util.Collection;
import java.util.Collections;
import java.util.HashSet;
import java.util.LinkedList;
import java.util.List;
import java.util.Set;

import org.apache.commons.collections.CollectionUtils;
import org.apache.commons.lang.StringUtils;
import org.sipfoundry.sipxconfig.admin.ExtensionInUseException;
import org.sipfoundry.sipxconfig.admin.NameInUseException;
import org.sipfoundry.sipxconfig.admin.commserver.Location;
import org.sipfoundry.sipxconfig.admin.commserver.LocationsManager;
import org.sipfoundry.sipxconfig.admin.commserver.SipxProcessContext;
import org.sipfoundry.sipxconfig.admin.commserver.SipxReplicationContext;
import org.sipfoundry.sipxconfig.admin.commserver.imdb.DataSet;
import org.sipfoundry.sipxconfig.admin.forwarding.AliasMapping;
import org.sipfoundry.sipxconfig.alias.AliasManager;
import org.sipfoundry.sipxconfig.common.BeanId;
import org.sipfoundry.sipxconfig.common.DaoUtils;
import org.sipfoundry.sipxconfig.common.SipUri;
import org.sipfoundry.sipxconfig.common.SipxHibernateDaoSupport;
import org.sipfoundry.sipxconfig.common.User;
import org.sipfoundry.sipxconfig.common.UserException;
import org.sipfoundry.sipxconfig.domain.DomainManager;
import org.sipfoundry.sipxconfig.freeswitch.FreeswitchAction;
import org.sipfoundry.sipxconfig.freeswitch.FreeswitchCondition;
import org.sipfoundry.sipxconfig.service.ServiceConfigurator;
import org.sipfoundry.sipxconfig.service.SipxFreeswitchService;
import org.sipfoundry.sipxconfig.service.SipxServiceManager;
import org.springframework.dao.support.DataAccessUtils;

public class OpenAcdContextImpl extends SipxHibernateDaoSupport implements OpenAcdContext {

    private static final String VALUE = "value";
    private static final String LOCATION = "location";
    private static final String OPEN_ACD_EXTENSION_WITH_NAME = "openAcdExtensionWithName";
    private static final String OPEN_ACD_AGENT_GROUP_WITH_NAME = "openAcdAgentGroupWithName";
    private static final String OPEN_ACD_SKILL_WITH_NAME = "openAcdSkillWithName";
    private static final String OPEN_ACD_SKILL_WITH_ATOM = "openAcdSkillWithAtom";
    private static final String DEFAULT_OPEN_ACD_SKILLS = "defaultOpenAcdSkills";
    private static final String ALIAS_RELATION = "openacd";
    private static final String OPEN_ACD_AGENT_BY_USERID = "openAcdAgentByUserId";
    private static final String AGENT_GROUP_NAME_DEFAULT = "Default";
    private static final String LINE_NAME = "line";

    private DomainManager m_domainManager;
    private SipxServiceManager m_serviceManager;
    private boolean m_enabled = true;
    private AliasManager m_aliasManager;
    private OpenAcdProvisioningContext m_provisioningContext;
    private LocationsManager m_locationsManager;
    private ServiceConfigurator m_serviceConfigurator;
    private SipxProcessContext m_processContext;
    private SipxReplicationContext m_replicationContext;

    @Override
    public OpenAcdExtension getExtensionById(Integer extensionId) {
        return getHibernateTemplate().load(OpenAcdExtension.class, extensionId);
    }

    @Override
    public OpenAcdExtension getExtensionByName(String extensionName) {
        List<OpenAcdLine> extensions = getHibernateTemplate().findByNamedQueryAndNamedParam(
                OPEN_ACD_EXTENSION_WITH_NAME, VALUE, extensionName);
        return DataAccessUtils.singleResult(extensions);
    }

    @Override
    public List<OpenAcdExtension> getFreeswitchExtensions() {
        return getHibernateTemplate().loadAll(OpenAcdExtension.class);
    }

    @Override
    public Set<OpenAcdLine> getLines(Location location) {
        List<OpenAcdLine> openacdLines = getHibernateTemplate().findByNamedQueryAndNamedParam(
                "openAcdLinesByLocationId", LOCATION, location);
        Set<OpenAcdLine> lines = new HashSet<OpenAcdLine>();
        for (OpenAcdLine ext : openacdLines) {
            if (ext.getExtension() != null) {
                lines.add(ext);
            }
        }
        return lines;
    }

    public Set<OpenAcdCommand> getCommands(Location location) {
        List<OpenAcdCommand> openacdCommands = getHibernateTemplate().findByNamedQueryAndNamedParam(
                "openAcdCommandsByLocationId", LOCATION, location);
        Set<OpenAcdCommand> comms = new HashSet<OpenAcdCommand>();
        for (OpenAcdCommand ext : openacdCommands) {
            if (ext.getExtension() != null) {
                comms.add(ext);
            }
        }
        return comms;
    }

    @Override
    public String[] getOpenAcdApplicationNames() {
        Set<String> applications = new HashSet<String>();
        List<FreeswitchAction> actions = getHibernateTemplate().loadAll(FreeswitchAction.class);
        for (FreeswitchAction freeswitchAction : actions) {
            applications.add(freeswitchAction.getApplication());
        }
        applications.addAll(FreeswitchAction.PredefinedAction.valuesAsStrings());
        return applications.toArray(new String[0]);
    }

    @Override
    public boolean isEnabled() {
        return m_enabled;
    }

    @Override
    public void removeExtensions(Collection<Integer> extensionIds) {
        removeAll(OpenAcdExtension.class, extensionIds);
        replicateConfig();
    }

    public void saveExtension(OpenAcdExtension extension) {
        if (extension.getName() == null) {
            throw new UserException("&null.name");
        }
        if (extension.getExtension() == null) {
            throw new UserException("&null.extension");
        }
        if (extension.isNew() || (!extension.isNew() && isNameChanged(extension))) {
            if (!m_aliasManager.canObjectUseAlias(extension, extension.getName())) {
                throw new NameInUseException(LINE_NAME, extension.getName());
            }
        } else if ((!extension.isNew() && isExtensionChanged(extension))) {
            if (extension.getExtension() != null
                    && !m_aliasManager.canObjectUseAlias(extension, extension.getExtension())) {
                throw new ExtensionInUseException(LINE_NAME, extension.getExtension());
            }
        }
        removeNullActions(extension);
        if (extension.isNew()) {
            getHibernateTemplate().save(extension);
        } else {
            getHibernateTemplate().merge(extension);
        }
        getHibernateTemplate().flush();
        replicateConfig();
    }

    private void replicateConfig() {
        m_replicationContext.generate(DataSet.ALIAS);
        SipxFreeswitchService freeswitchService = (SipxFreeswitchService) m_serviceManager
                .getServiceByBeanId(SipxFreeswitchService.BEAN_ID);
        for (Location location : m_locationsManager.getLocations()) {
            if (location.isServiceInstalled(freeswitchService)) {
                m_serviceConfigurator.replicateServiceConfig(location, freeswitchService, true, false);
            }
        }
        m_processContext.markServicesForReload(Collections.singleton(freeswitchService));
    }

    private void removeNullActions(OpenAcdExtension extension) {
        if (extension.getConditions() == null) {
            return;
        }
        for (FreeswitchCondition condition : extension.getConditions()) {
            for (FreeswitchAction action : condition.getActions()) {
                if (action != null && action.getApplication() != null) {
                    condition.addAction(action);
                }
            }
        }
    }

    private boolean isNameChanged(OpenAcdExtension extension) {
        return !getExtensionById(extension.getId()).getName().equals(extension.getName());
    }

    private boolean isExtensionChanged(OpenAcdExtension extension) {
        return !getExtensionById(extension.getId()).getExtension().equals(extension.getExtension());
    }

    @Override
    public Collection getBeanIdsOfObjectsWithAlias(String alias) {
        Collection bids = new ArrayList<BeanId>();

        List<OpenAcdExtension> extensions = getFreeswitchExtensions();
        for (OpenAcdExtension openAcdExtension : extensions) {
            if (openAcdExtension.getExtension() != null
                    && (openAcdExtension.getExtension().equals(alias) || openAcdExtension.getName().equals(alias))) {
                bids.add(new BeanId(openAcdExtension.getId(), OpenAcdLine.class));
            }
        }
        return bids;
    }

    @Override
    public boolean isAliasInUse(String alias) {
        List<OpenAcdExtension> extensions = getFreeswitchExtensions();
        for (OpenAcdExtension openAcdExtension : extensions) {
            if (openAcdExtension.getExtension() != null
                    && (openAcdExtension.getExtension().equals(alias) || openAcdExtension.getName().equals(alias))) {
                return true;
            }
        }
        return false;
    }

    @Override
    public Collection<AliasMapping> getAliasMappings() {
        List<OpenAcdExtension> extensions = getHibernateTemplate().loadAll(OpenAcdExtension.class);
        final ArrayList<AliasMapping> list = new ArrayList<AliasMapping>();
        String domainName = m_domainManager.getDomain().getName();
        for (OpenAcdExtension extension : extensions) {
            if (extension.getExtension() != null) {
                list.addAll(generateAlias(extension, domainName));
            }
        }
        return list;
    }

    private List<AliasMapping> generateAlias(OpenAcdExtension extension, String domainName) {
        List<AliasMapping> mappings = new ArrayList<AliasMapping>();
        SipxFreeswitchService freeswitchService = (SipxFreeswitchService) m_serviceManager
                .getServiceByBeanId(SipxFreeswitchService.BEAN_ID);
        AliasMapping nameMapping = new AliasMapping();
        nameMapping.setIdentity(AliasMapping.createUri(extension.getName(), domainName));
        nameMapping.setContact(SipUri.format(extension.getExtension(), freeswitchService.getAddress(), false));
        nameMapping.setRelation(ALIAS_RELATION);
        mappings.add(nameMapping);
        AliasMapping lineMapping = new AliasMapping();
        lineMapping.setIdentity(AliasMapping.createUri(extension.getExtension(), domainName));
        lineMapping.setContact(SipUri.format(extension.getExtension(), freeswitchService.getAddress(),
                freeswitchService.getFreeswitchSipPort()));
        lineMapping.setRelation(ALIAS_RELATION);
        mappings.add(lineMapping);
        return mappings;
    }

    public List<OpenAcdAgentGroup> getAgentGroups() {
        return getHibernateTemplate().loadAll(OpenAcdAgentGroup.class);
    }

    public OpenAcdAgentGroup getAgentGroupById(Integer agentGroupId) {
        return getHibernateTemplate().load(OpenAcdAgentGroup.class, agentGroupId);
    }

    public OpenAcdAgentGroup getAgentGroupByName(String agentGroupName) {
        List<OpenAcdAgentGroup> agentGroups = getHibernateTemplate().findByNamedQueryAndNamedParam(
                OPEN_ACD_AGENT_GROUP_WITH_NAME, VALUE, agentGroupName);
        return DataAccessUtils.singleResult(agentGroups);
    }

    public void saveAgentGroup(OpenAcdAgentGroup agentGroup) {
        // check if agent group name is empty
        if (StringUtils.isBlank(agentGroup.getName())) {
            throw new UserException("&blank.agentGroupName.error");
        }
        // Check for duplicate names before saving the agent group
        if (agentGroup.isNew() || (!agentGroup.isNew() && isNameChanged(agentGroup))) {
            checkForDuplicateName(agentGroup);
        }

        if (!agentGroup.isNew()) {
            if (isNameChanged(agentGroup)) {
                // don't rename the default group
                OpenAcdAgentGroup defaultAgentGroup = getAgentGroupByName(AGENT_GROUP_NAME_DEFAULT);
                if (defaultAgentGroup != null && defaultAgentGroup.getId().equals(agentGroup.getId())) {
                    throw new UserException("&msg.err.defaultAgentGroupRename");
                }
            }
            getHibernateTemplate().merge(agentGroup);
            m_provisioningContext.updateObjects(Collections.singletonList(agentGroup));
        } else {
            getHibernateTemplate().save(agentGroup);
            m_provisioningContext.addObjects(Collections.singletonList(agentGroup));
        }
    }

    private boolean isNameChanged(OpenAcdAgentGroup agentGroup) {
        String oldName = getAgentGroupById(agentGroup.getId()).getName();
        agentGroup.setOldName(oldName);
        return !oldName.equals(agentGroup.getName());
    }

    private void checkForDuplicateName(OpenAcdAgentGroup agentGroup) {
        String agentGroupName = agentGroup.getName();
        OpenAcdAgentGroup existingAgentGroup = getAgentGroupByName(agentGroupName);
        if (existingAgentGroup != null) {
            throw new UserException("&duplicate.agentGroupName.error", agentGroupName);
        }
    }

    public boolean removeAgentGroups(Collection<Integer> agentGroupIds) {
        boolean affectDefaultAgentGroup = false;
        List<OpenAcdAgentGroup> groups = new LinkedList<OpenAcdAgentGroup>();
        List<OpenAcdAgent> agents = new LinkedList<OpenAcdAgent>();
        for (Integer id : agentGroupIds) {
            OpenAcdAgentGroup group = getAgentGroupById(id);
            if (!group.getName().equals(AGENT_GROUP_NAME_DEFAULT)) {
                agents.addAll(group.getAgents());
                groups.add(group);
            } else {
                affectDefaultAgentGroup = true;
            }
        }
        getHibernateTemplate().deleteAll(groups);
        m_provisioningContext.deleteObjects(agents);
        m_provisioningContext.deleteObjects(groups);

        return affectDefaultAgentGroup;
    }

    public List<OpenAcdAgent> addAgentsToGroup(OpenAcdAgentGroup agentGroup, Collection<OpenAcdAgent> agents) {
        List<OpenAcdAgent> existingAgents = new ArrayList<OpenAcdAgent>();
        for (OpenAcdAgent agent : agents) {
            if (!isOpenAcdAgent(agent.getUser())) {
                agentGroup.addAgent(agent);
                agent.setGroup(agentGroup);
            } else {
                existingAgents.add(agent);
            }
        }
        // update the group
        saveAgentGroup(agentGroup);
        m_provisioningContext.addObjects(new LinkedList<OpenAcdConfigObject>(CollectionUtils.subtract(agents,
                existingAgents)));

        return existingAgents;
    }

    public List<OpenAcdAgent> getAgents() {
        return getHibernateTemplate().loadAll(OpenAcdAgent.class);
    }

    public OpenAcdAgent getAgentById(Integer agentId) {
        return getHibernateTemplate().load(OpenAcdAgent.class, agentId);
    }

    public OpenAcdAgent getAgentByUserId(Integer userId) {
        Collection<OpenAcdAgent> agents = getHibernateTemplate().findByNamedQueryAndNamedParam(
                OPEN_ACD_AGENT_BY_USERID, VALUE, userId);
        return DaoUtils.requireOneOrZero(agents, OPEN_ACD_AGENT_BY_USERID);
    }

    public void saveAgent(OpenAcdAgentGroup agentGroup, OpenAcdAgent agent) {
        checkAgent(agent);
        addAgentsToGroup(agentGroup, Collections.singletonList(agent));
    }

    @Override
    public void saveAgent(OpenAcdAgent agent) {
        checkAgent(agent);
        getHibernateTemplate().saveOrUpdate(agent);
        agent.setOldName(agent.getName());
        m_provisioningContext.updateObjects(Collections.singletonList(agent));
    }

    private void checkAgent(OpenAcdAgent agent) {
        if (StringUtils.isBlank(agent.getPin())) {
            throw new UserException("&blank.agentPin.error");
        }
        // check if agent security is empty
        if (StringUtils.isBlank(agent.getSecurity())) {
            throw new UserException("&blank.agentSecurity.error");
        }
    }

    @Override
    public void deleteAgents(Integer groupId, Collection<Integer> agentIds) {
        OpenAcdAgentGroup group = getAgentGroupById(groupId);
        List<OpenAcdAgent> agents = new LinkedList<OpenAcdAgent>();
        for (Integer id : agentIds) {
            OpenAcdAgent agent = getAgentById(id);
            group.removeAgent(agent);
            agents.add(agent);
        }
        getHibernateTemplate().save(group);
        m_provisioningContext.deleteObjects(agents);
    }

    @Override
    public OpenAcdAgent getAgentByUser(User user) {
        return getAgentByUserId(user.getId());
    }

    @Override
    public boolean isOpenAcdAgent(User user) {
        if (getAgentByUser(user) != null) {
            return true;
        }
        return false;
    }

    @Override
    public List<OpenAcdSkill> getSkills() {
        return getHibernateTemplate().loadAll(OpenAcdSkill.class);
    }

    @Override
    public List<OpenAcdSkill> getDefaultSkills() {
        return getHibernateTemplate().findByNamedQuery(DEFAULT_OPEN_ACD_SKILLS);
    }

    @Override
    public OpenAcdSkill getSkillById(Integer skillId) {
        return getHibernateTemplate().load(OpenAcdSkill.class, skillId);
    }

    @Override
    public OpenAcdSkill getSkillByName(String skillName) {
        List<OpenAcdSkill> skills = getHibernateTemplate().findByNamedQueryAndNamedParam(OPEN_ACD_SKILL_WITH_NAME,
                VALUE, skillName);
        return DataAccessUtils.singleResult(skills);
    }

    @Override
    public OpenAcdSkill getSkillByAtom(String atom) {
        List<OpenAcdSkill> skills = getHibernateTemplate().findByNamedQueryAndNamedParam(OPEN_ACD_SKILL_WITH_ATOM,
                VALUE, atom);
        return DataAccessUtils.singleResult(skills);
    }

    @Override
    public void saveSkill(OpenAcdSkill skill) {
        // Check if skill name is empty
        if (StringUtils.isBlank(skill.getName())) {
            throw new UserException("&blank.skillName.error");
        }
        // Check if skill atom is empty
        if (StringUtils.isBlank(skill.getAtom())) {
            throw new UserException("&blank.skillAtom.error");
        }
        // Check if skill group name is empty
        if (StringUtils.isBlank(skill.getGroupName())) {
            throw new UserException("&blank.skillGroupName.error");
        }
        // Check for duplicate names before saving the skill
        if (skill.isNew() || (!skill.isNew() && isNameChanged(skill))) {
            checkForDuplicateName(skill);
        }
        // Check for duplicate atoms before saving the skill
        if (skill.isNew() || (!skill.isNew() && isAtomChanged(skill))) {
            checkForDuplicateAtom(skill);
        }

        if (!skill.isNew()) {
            getHibernateTemplate().merge(skill);
            m_provisioningContext.updateObjects(Collections.singletonList(skill));
        } else {
            getHibernateTemplate().save(skill);
            m_provisioningContext.addObjects(Collections.singletonList(skill));
        }
    }

    public boolean removeSkills(Collection<Integer> skillIds) {
        boolean affectDefaultSkills = false;
        List<OpenAcdSkill> skills = new LinkedList<OpenAcdSkill>();
        for (Integer id : skillIds) {
            OpenAcdSkill skill = getSkillById(id);
            if (!skill.isDefaultSkill()) {
                skills.add(skill);
            } else {
                affectDefaultSkills = true;
            }
        }
        getHibernateTemplate().deleteAll(skills);
        m_provisioningContext.deleteObjects(skills);

        return affectDefaultSkills;
    }

    private boolean isNameChanged(OpenAcdSkill skill) {
        return !getSkillById(skill.getId()).getName().equals(skill.getName());
    }

    private void checkForDuplicateName(OpenAcdSkill skill) {
        String skillName = skill.getName();
        OpenAcdSkill existingSkill = getSkillByName(skillName);
        if (existingSkill != null) {
            throw new UserException("&duplicate.skillName.error", existingSkill);
        }
    }

    private boolean isAtomChanged(OpenAcdSkill skill) {
        return !getSkillById(skill.getId()).getAtom().equals(skill.getAtom());
    }

    private void checkForDuplicateAtom(OpenAcdSkill skill) {
        String atom = skill.getAtom();
        OpenAcdSkill existingSkill = getSkillByAtom(atom);
        if (existingSkill != null) {
            throw new UserException("&duplicate.skillAtom.error", existingSkill);
        }
    }

    public void setDomainManager(DomainManager manager) {
        m_domainManager = manager;
    }

    public void setSipxServiceManager(SipxServiceManager manager) {
        m_serviceManager = manager;
    }

    public void setAliasManager(AliasManager aliasManager) {
        m_aliasManager = aliasManager;
    }

    public void setProvisioningContext(OpenAcdProvisioningContext context) {
        m_provisioningContext = context;
    }

    public void setServiceConfigurator(ServiceConfigurator serviceConfigurator) {
        m_serviceConfigurator = serviceConfigurator;
    }

    public void setProcessContext(SipxProcessContext processContext) {
        m_processContext = processContext;
    }

    public void setLocationsManager(LocationsManager locationsManager) {
        m_locationsManager = locationsManager;
    }

    public void setReplicationContext(SipxReplicationContext replicationContext) {
        m_replicationContext = replicationContext;
    }

}
